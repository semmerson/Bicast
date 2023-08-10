/**
 * BSD Sockets.
 *
 *        File: Socket.cpp
 *  Created on: May 9, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include "error.h"
#include "InetAddr.h"
#include "Shield.h"
#include "Socket.h"
#include "Stopwatch.h"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <mutex>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

namespace hycast {

/// Implementation of a socket
class Socket::Impl
{
    Impl()
        : mutex()
        , rmtSockAddr()
        , sd{-1}
        , domain{AF_UNSPEC}
        , shutdownCalled{false}
    {}

    /**
     * @param[in] sd  Socket descriptor
     */
    void init(const int sd)
    {
        //LOG_DEBUG("Initializing socket descriptor %d", sd);
        this->sd = sd;

        struct sockaddr_storage storage = {};
        socklen_t               socklen = sizeof(storage);

        if (::getpeername(sd, reinterpret_cast<struct sockaddr*>(&storage), &socklen) == 0)
            rmtSockAddr = SockAddr(storage);
    }

protected:
    using Mutex   = std::mutex;             ///< Type of mutex for maintaining consistency
    using Guard   = std::lock_guard<Mutex>; ///< Type of lock guard

    mutable Mutex         mutex;           ///< Mutex for maintaining consistency
    SockAddr              rmtSockAddr;     ///< Socket address of remote endpoint
    int                   sd;              ///< Socket descriptor
    int                   domain;          ///< IP domain: AF_INET, AF_INET6
    bool                  shutdownCalled;  ///< `shutdown()` has been called?
    mutable unsigned      bytesWritten =0; ///< Number of bytes written
    mutable unsigned      bytesRead =0;    ///< Number of bytes read
    static const uint64_t writePad;        ///< Write alignment buffer
    static uint64_t       readPad;         ///< Read alignment buffer

    /**
     * Constructs a server-side socket. Closes the socket descriptor on destruction.
     *
     * @param[in] sd  Socket descriptor
     */
    Impl(const int sd)
        : Impl()
    {
        Shield shield{};
        init(sd);
    }

    /**
     * Constructs an unbound and unconnected socket of a given address family, type, and protocol.
     *
     * @param[in] family    Address family (e.g., `AF_INET`, `AF_INET6`)
     * @param[in] type      Type of socket (e.g., `SOCK_STREAM`, `SOCK_DGRAM`, `SOCK_SEQPACKET`)
     * @param[in] protocol  Socket protocol (e.g., `IPPROTO_TCP`, `IPPROTO_UDP`; 0 obtains default
     *                      for type)
     * @throw SystemError   Couldn't create socket
     */
    Impl(   const int  family,
            const int  type,
            const int  protocol)
        : Impl()
    {
        sd = ::socket(family, type, protocol);
        if (sd == -1)
            throw SYSTEM_ERROR("Couldn't create socket {family=" + std::to_string(family) +
                    ", type=" + std::to_string(type) + ", proto=" + std::to_string(protocol) + "}");
        domain = family;
    }

    /**
     * Constructs.
     *
     * @param[in] inetAddr  IP address. May be wildcard.
     * @param[in] type      Type of socket (e.g., SOCK_STREAM)
     * @param[in] protocol  Socket protocol (e.g., IPPROTO_TCP; 0 obtains default for type)
     */
    Impl(   const InetAddr inetAddr,
            const int      type,
            const int      protocol) noexcept
        : Impl()
    {
        Shield shield{};
        init(inetAddr.socket(type, protocol));
        domain = inetAddr.getFamily();
    }

    /**
     * Returns the minimum number of padding bytes needed to align the next I/O action.
     * @param[in] nbytes  Number of bytes already read or written
     * @param[in] align   Number to align the next action to
     * @return            The minimum number of padding bytes
     */
    static inline size_t padLen(
            const unsigned nbytes,
            const size_t   align) {
        return (align - (nbytes % align)) % align;
        /*
         * Alternative?
         * See <https://en.wikipedia.org/wiki/Data_structure_alignment#Computing_padding>.
         *
         * Works for unsigned and two's-complement `nbytes` but not one's-complement nor
         * sign-magnitude
         *
         * padding = (align - (nbytes & (align - 1))) & (align - 1)
         *         = -nbytes & (align - 1)
         */
    }

    /**
     * Writes zero or more bytes in order to align the next write.
     * @param[in] align    Number of bytes in the next write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    inline bool alignWriteTo(size_t align)
    {
        //LOG_DEBUG("bytesWritten=%s, align=%s",
                //std::to_string(bytesWritten).data(),
                //std::to_string(align).data());
        const auto nbytes = padLen(bytesWritten, align);
        return nbytes
                ? write(&writePad, nbytes)
                : true;
    }

    /**
     * Aligns the next read.
     * @param[in] align    Number of bytes to align to
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    inline bool alignReadTo(size_t align)
    {
        const auto nbytes = padLen(bytesRead, align);
        return nbytes
                ? read(&readPad, nbytes)
                : true;
    }

    /**
     * Writes bytes in an implementation-specific manner. Doesn't modify the
     * number of bytes written.
     *
     * @param[in] data      Bytes to write.
     * @param[in] nbytes    Number of bytes to write.
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool writeBytes(const void* data,
                            size_t      nbytes) =0;

    /**
     * Reads bytes in an implementation-specific manner. Doesn't modify the
     * number of bytes read.
     *
     * @param[out] data         Destination buffer
     * @param[in]  nbytes       Number of bytes to read
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    virtual bool readBytes(void* const data,
                           size_t      nbytes) =0;

    /**
     * Idempotent.
     *
     * @pre                `mutex` is locked
     * @param[in] what     What to shut down. One of `SHUT_RD`, `SHUT_WR`, or
     *                     `SHUT_RDWR`.
     * @throw SystemError  Couldn't shutdown socket
     */
    void shut(const int what) {
        //LOG_DEBUG("Shutting down socket %s", std::to_string(sd).data());
        if (::shutdown(sd, what) && errno != ENOTCONN)
            throw SYSTEM_ERROR("::shutdown failure on socket " + std::to_string(sd));
        shutdownCalled = true;
    }

public:
    virtual ~Impl() noexcept {
        //LOG_DEBUG("Closing socket descriptor %d", sd);
        ::close(sd);
    }

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    size_t hash() const noexcept {
        return getLclAddr().hash() ^ getRmtAddr().hash();
    }

    /**
     * Indicates if this instance is less than another.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is less than the other
     * @retval    false    This instance is not less than the other
     */
    bool operator<(const Impl& rhs) const noexcept {
        auto lhsAddr = getLclAddr();
        auto rhsAddr = rhs.getLclAddr();

        if (lhsAddr < rhsAddr)
            return true;
        if (rhsAddr < lhsAddr)
            return false;

        lhsAddr = getRmtAddr();
        rhsAddr = rhs.getRmtAddr();

        if (lhsAddr < rhsAddr)
            return true;

        return false;
    }

    /**
     * Indicates if this instance is valid.
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const {
        Guard guard{mutex};
        return !shutdownCalled && sd >= 0;
    }

    /**
     * Returns the socket descriptor.
     *
     * @return Socket descriptor
     */
    int getSockDesc() const {
        return sd;
    }

    /**
     * Returns the socket address of the local endpoint.
     * @return The socket address of the local endpoint
     */
    SockAddr getLclAddr() const {
        struct sockaddr_storage storage = {};
        socklen_t               socklen = sizeof(storage);

        if (::getsockname(sd, reinterpret_cast<struct sockaddr*>(&storage), &socklen))
            throw SYSTEM_ERROR("getsockname() failure on socket " + std::to_string(sd));

        return SockAddr(storage);
    }

    /**
     * Returns the socket address of the remote endpoint.
     * @return The socket address of the remote endpoint
     */
    SockAddr getRmtAddr() const noexcept {
        return rmtSockAddr;
    }

    /**
     * Returns the port number of the local endpoint.
     * @return The port number of the local endpoint
     */
    in_port_t getLclPort() const {
        return getLclAddr().getPort();
    }

    /**
     * Returns the port number of the remote endpoint.
     * @return The port number of the remote endpoint
     */
    in_port_t getRmtPort() const noexcept {
        return getRmtAddr().getPort();
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    virtual std::string to_string() const =0;

    /**
     * Assigns this instance a local socket address.
     *
     * @param[in] lclAddr    Local socket address. Note that, for multicast reception, this will be
     *                       the socket address of the multicast group.
     * @return               This instance
     * @throw   SystemError  System failure
     */
    Impl& bind(const SockAddr lclAddr) {
        sockaddr_storage storage;
        lclAddr.get_sockaddr(storage);
        //LOG_DEBUG("Binding socket %s to %s", std::to_string(sd).data(), lclAddr.to_string().data());
        if (::bind(sd, reinterpret_cast<struct sockaddr*>(&storage), sizeof(storage)))
            throw SYSTEM_ERROR("Couldn't bind socket " + std::to_string(sd) + " to " + to_string());
        return *this;
    }

    /**
     * Makes this instance non-blocking.
     *
     * @return  This instance.
     * @throw   SystemError  System failure
     */
    Impl& makeNonBlocking() {
        // Get current socket flags
        int currSockFlags = ::fcntl(sd, F_GETFL, 0);
        if (currSockFlags < 0)
            throw SYSTEM_ERROR("Couldn't get socket flags");

        // Set O_NONBLOCK
        if (::fcntl(sd, F_SETFL, currSockFlags | O_NONBLOCK) == -1)
            throw SYSTEM_ERROR("Couldn't make socket non-blocking");

        return *this;
    }

    /**
     * Makes this instance blocking.
     *
     * @return  This instance.
     * @throw   SystemError  System failure
     */
    Impl& makeBlocking() {
        // Get current socket flags
        int currSockFlags = ::fcntl(sd, F_GETFL, 0);
        if (currSockFlags < 0)
            throw SYSTEM_ERROR("Couldn't get socket flags");

        // Set O_NONBLOCK
        if (::fcntl(sd, F_SETFL, currSockFlags & ~O_NONBLOCK) == -1)
            throw SYSTEM_ERROR("Couldn't make socket blocking");

        return *this;
    }

    /**
     * Connects this instance to a remote socket address.
     *
     * @param[in]  rmtAddr   Remote socket address
     * @return               This instance
     * @throw   SystemError  System failure
     */
    Impl& connect(const SockAddr rmtAddr) {
        struct sockaddr_storage storage;
        if (::connect(sd, rmtAddr.get_sockaddr(storage), sizeof(storage)) && errno != EINPROGRESS)
            throw SYSTEM_ERROR("connect() failure");
        rmtSockAddr = rmtAddr;
        return *this;
    }

    /**
     * Writes bytes.
     *
     * @param[in] data          Bytes to write.
     * @param[in] nbytes        Number of bytes to write.
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    bool write(const void*  data,
               const size_t nbytes) {
        //LOG_DEBUG("Writing %zu bytes to %s", nbytes, to_string().data());
        if (writeBytes(data, nbytes)) {
            bytesWritten += nbytes;
            return true;
        }
        return false;
    }

    /**
     * Performs value-alignment.
     */
    template<class TYPE>
    bool write(TYPE value) {
        return alignWriteTo(sizeof(value)) && write(&value, sizeof(value));
    }

    /**
     * Flushes this instance. Does nothing for TCP. Writes a datagram for UDP.
     * @retval true     Success
     * @retval false    Lost connection
     */
    virtual bool flush() =0;

    /**
     * Reads bytes.
     *
     * @param[out] data         Destination buffer
     * @param[in]  nbytes       Number of bytes to read
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    bool read(void* const  data,
              const size_t nbytes) {
        if (readBytes(data, nbytes)) {
            bytesRead += nbytes;
            //LOG_DEBUG("Read %zu bytes from %s", nbytes, to_string().data());
            return true;
        }
        return false;
    }

    /**
     * Readies this instance for new input. Does nothing if a TCP connection. Clears the input
     * buffer if a UDP connection.
     */
    virtual void clear() =0;

    /**
     * Performs value-alignment.
     */
    template<class TYPE>
    bool read(TYPE& value) {
        return alignReadTo(sizeof(value)) && read(&value, sizeof(value));
    }

    /**
     * Reads a string.
     * @tparam     UINT     The type of integer encoding the number of characters in the string
     * @param[out] string   The string to be set
     * @retval     true     Success
     * @retval     false    Success
     */
    template<typename UINT>
    bool read(std::string& string) {
        UINT size;
        if (read<UINT>(size)) {
            char bytes[size];
            if (read(bytes, size)) {
                string.assign(bytes, size);
                return true;
            }
        }
        return false;
    }

    /**
     * Shuts down the socket. If reading is shut down, then `accept()` will
     * return `nullptr` and `read()` will return false. Idempotent.
     *
     * @param what          What to shut down. One of `SHUT_RD`, `SHUT_WR`, or
     *                      `SHUT_RDWR`
     * @throws SystemError  Couldn't shutdown socket
     */
    void shutdown(const int what) {
        Guard guard{mutex};
        shut(what);
    }

    /**
     * Indicates if this instance is shut down.
     * @retval true     This instance is shut down
     * @retval false    This instance is not shut down
     */
    bool isShutdown() const {
        Guard guard{mutex};
        return shutdownCalled;
    }
};

const uint64_t Socket::Impl::writePad = 0;  ///< Write alignment buffer
uint64_t Socket::Impl::readPad;  ///< Read alignment buffer

Socket::Socket(Impl* impl)
    : pImpl(impl)
{}

Socket::~Socket()
{}

Socket::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

size_t Socket::hash() const noexcept {
    return pImpl ? pImpl->hash() : 0;
}

std::string Socket::to_string() const {
    return pImpl ? pImpl->to_string() : "<unset>";
}

bool Socket::operator<(const Socket& rhs) const noexcept {
    auto impl1 = pImpl.get();
    auto impl2 = rhs.pImpl.get();

    return (impl1 == impl2)
            ? false
            : (impl1 == nullptr || impl2 == nullptr)
                  ? (impl1 == nullptr)
                  : *impl1 < *impl2;
}

void Socket::swap(Socket& socket) noexcept {
    pImpl.swap(socket.pImpl);
}

int Socket::getSockDesc() const {
    return pImpl->getSockDesc();
}

SockAddr Socket::getLclAddr() const {
    return pImpl->getLclAddr();
}

in_port_t Socket::getLclPort() const {
    return pImpl->getLclPort();
}

SockAddr Socket::getRmtAddr() const noexcept {
    return pImpl->getRmtAddr();
}

in_port_t Socket::getRmtPort() const {
    return pImpl->getRmtPort();
}

Socket& Socket::bind(const SockAddr lclAddr) {
    pImpl->bind(lclAddr);
    return *this;
}

Socket& Socket::makeNonBlocking() {
    pImpl->makeNonBlocking();
    return *this;
}

Socket& Socket::makeBlocking() {
    pImpl->makeBlocking();
    return *this;
}

Socket& Socket::connect(const SockAddr rmtAddr) {
    pImpl->connect(rmtAddr);
    return *this;
}

bool Socket::write(const void*  data,
                   const size_t nbytes) const {
    return pImpl->write(data, nbytes);
}
bool Socket::write(const bool value) const {
    return pImpl->write<bool>(value);
}
bool Socket::write(const uint8_t value) const {
    return pImpl->write<uint8_t>(value);
}
bool Socket::write(const uint16_t value) const {
    return pImpl->write<uint16_t>(value);
}
bool Socket::write(const uint32_t value) const {
    return pImpl->write<uint32_t>(value);
}
bool Socket::write(const uint64_t value) const {
    return pImpl->write<uint64_t>(value);
}

bool Socket::flush() {
    return pImpl->flush();
}

void Socket::clear() {
    return pImpl->clear();
}

bool Socket::read(void*        data,
                  const size_t nbytes) const {
    return pImpl->read(data, nbytes);
}
bool Socket::read(bool& value) const {
    return pImpl->read<bool>(value);
}
bool Socket::read(uint8_t& value) const {
    return pImpl->read<uint8_t>(value);
}
bool Socket::read(uint16_t& value) const {
    return pImpl->read<uint16_t>(value);
}
bool Socket::read(uint32_t& value) const {
    return pImpl->read<uint32_t>(value);
}
bool Socket::read(uint64_t& value) const {
    return pImpl->read<uint64_t>(value);
}
template<typename UINT>
bool Socket::read(std::string& string) const {
    return pImpl->read<UINT>(string);
}

void Socket::shutdown(const int what) const
{
    pImpl->shutdown(what);
}

bool Socket::isShutdown() const
{
    return pImpl->isShutdown();
}

/******************************************************************************/

/// Implementation of a TCP socket
class TcpSock::Impl : public Socket::Impl
{
protected:
    Impl();

    /**
     * Writes to the socket. No host-to-network translation is performed.
     *
     * @param[in] data         Bytes to write
     * @param[in] nbytes       Number of bytes to write
     * @retval    false        Connection is closed
     * @retval    true         Success
     * @throws    SystemError  System error
     */
    bool writeBytes(const void* data,
                    size_t      nbytes) override {
        //LOG_DEBUG("Writing %zu bytes", nbytes);

        const char*   bytes = static_cast<const char*>(data);
        struct pollfd pollfd;

        pollfd.fd = sd;
        pollfd.events = POLLOUT;

        while (nbytes) {
            /*
             * poll(2) is used because SIGPIPE was always delivered by the
             * development system even if it was explicitly ignored.
             */
            if (::poll(&pollfd, 1, -1) == -1)
                throw SYSTEM_ERROR("poll() failure for socket " + to_string());
            if (pollfd.revents & POLLHUP)
                return false;
            if (pollfd.revents & (POLLOUT | POLLERR)) {
                // Cancellation point
                //LOG_DEBUG("sd=%d, bytes=%p, nbytes=%zu", sd, bytes, nbytes);
                auto nwritten = ::write(sd, bytes, nbytes);
                //LOG_DEBUG("nwritten=%zd", nwritten);

                if (nwritten == -1) {
                    if (errno == ECONNRESET || errno == EPIPE) {
                        return false;
                    }
                    throw SYSTEM_ERROR("write() failure to socket " +
                            to_string());
                }
                if (nwritten == 0)
                    return false;

                nbytes -= nwritten;
                bytes += nwritten;
            }
            else {
                throw RUNTIME_ERROR("poll() failure on socket " + to_string());
            }
        }

        return true;
    }

    /**
     * Reads from the socket. No network-to-host translation is performed.
     *
     * @param[out] data         Destination buffer
     * @param[in]  nbytes       Number of bytes to read
     * @retval     true         Success
     * @retval     false        EOF or `shutdown()` called
     * @throw      SystemError  I/O failure
     */
    bool readBytes(void* const  data,
                   const size_t nbytes) override {
        /*
         * Sending process closes connection => FIN sent => EOF
         * Sending process crashes => FIN sent => EOF
         * Sending host crashes => read() won't return
         * Sending host becomes unreachable => read() won't return
         */
        //LOG_DEBUG("Reading %zu bytes", nbytes);

        char*         bytes = static_cast<char*>(data);
        auto          nleft = nbytes;
        struct pollfd pollfd;

        pollfd.fd = sd;
        pollfd.events = POLLIN;

        while (nleft) {
            /*
             * poll(2) is used to learn if this end has closed the socket.
             */
            //LOG_DEBUG("Polling socket %s", std::to_string(sd).data());
            if (::poll(&pollfd, 1, -1) == -1)
                throw SYSTEM_ERROR("poll() failure on socket " + to_string());
            if (pollfd.revents & POLLHUP) {
                LOG_TRACE("EOF on socket " + std::to_string(sd));
                return false; // EOF
            }
            if (pollfd.revents & (POLLIN | POLLERR)) {
                auto nread = ::read(sd, bytes, nleft);

                if (nread == -1)
                    throw SYSTEM_ERROR("Couldn't read from socket " +
                            to_string());
                if (nread == 0) {
                    LOG_TRACE("EOF on socket " + std::to_string(sd));
                    return false; // EOF
                }

                nleft -= nread;
                bytes += nread;
            }
            else {
                throw RUNTIME_ERROR("poll() failure to socket " + to_string());
            }
        }

        return true;
    }

public:
    /**
     * Constructs.
     *
     * @param[in] inetAddr  Associated IP address. May be wildcard. Used to determine address
     *                      family.
     * @exceptionsafety     Strong guarantee
     * @cancellationpoint
     */
    explicit Impl(const InetAddr inetAddr)
        : Socket::Impl{inetAddr, SOCK_STREAM, IPPROTO_TCP}
    {}

    /**
     * Constructs.
     * @param[in] sd  The underlying socket descriptor
     */
    explicit Impl(const int sd)
        : Socket::Impl(sd)
    {}

    /**
     * Constructs.
     * @param[in] family  Address family: `AF_INET` or `AF_INET6`
     * @param dummy       Dummy argument to differentiate constructors
     */
    Impl(   const int family,
            const bool dummy)
        : Socket::Impl{family, SOCK_STREAM, IPPROTO_TCP}
    {}

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    virtual std::string to_string() const override
    {
        return "{sd=" + std::to_string(sd) + ", lcl=" + getLclAddr().to_string()
                + ", proto=TCP, rmt=" + getRmtAddr().to_string() + "}";
    }

    /**
     * Sets the Nagle algorithm.
     *
     * @param[in] enable             Whether or not to enable the Nagle
     *                               algorithm
     * @throws    std::system_error  `setsockopt()` failure
     */
    void setDelay(int enable)
    {
        if (::setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &enable,
                sizeof(enable))) {
            throw SYSTEM_ERROR("Couldn't set TCP_NODELAY to " +
                    std::to_string(enable) + " on socket " + to_string());
        }
    }

    bool flush() {
        return true;
    }

    void clear() {
    }
};

TcpSock::TcpSock(Impl* impl)
    : Socket(impl) {
}

TcpSock::~TcpSock() {
}

TcpSock& TcpSock::setDelay(bool enable) {
    static_cast<TcpSock::Impl*>(pImpl.get())->setDelay(enable);
    return *this;
}

/******************************************************************************/

/// Implementation of a TCP server socket
class TcpSrvrSock::Impl final : public TcpSock::Impl
{
public:
    /**
     * Constructs. Calls listen() on the created socket.
     *
     * @param[in] lclSockAddr        Server's local socket address. The IP address may be the
     *                               wildcard, in which case the server will listen on all
     *                               interfaces. The port number may be zero, in which case it will
     *                               be chosen by the operating system.
     * @param[in] queueSize          Size of listening queue
     * @throws    std::system_error  Couldn't create socket
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     * @throws    std::system_error  Couldn't listen on socket
     */
    Impl(   const SockAddr lclSockAddr,
            const int      queueSize)
        : TcpSock::Impl{lclSockAddr.getInetAddr()}
    {
        //LOG_DEBUG("Setting SO_REUSEADDR");
        const int enable = 1;
        if (::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_REUSEADDR on socket " + to_string());

        //LOG_NOTE("Binding socket %s to address %s", std::to_string(sd).data(),
                //lclSockAddr.to_string().data());
        lclSockAddr.bind(sd);

        //LOG_DEBUG("Setting SO_KEEPALIVE");
        if (::setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_KEEPALIVE on socket " + to_string());

        if (::listen(sd, queueSize))
            throw SYSTEM_ERROR("listen() failure on socket " + to_string() + ", queueSize=" +
                    std::to_string(queueSize));
    }

    std::string to_string() const override
    {
        return "{sd=" + std::to_string(sd) + ", lcl=" + getLclAddr().to_string() + ", proto=TCP}";
    }

    /**
     * Calls listen() on the underlying socket.
     * @param[in] size       Size of the listening queue
     * @throws  SystemError  System failure
     * @cancellationpoint    Yes
     */
    void listen(const int size) {
        if (::listen(sd, size))
            throw SYSTEM_ERROR("Couldn't set socket " + to_string() + " listen-queue size to " +
                    std::to_string(size));
    }

    /**
     * Accepts the next, incoming connection.
     *
     * @retval  `nullptr`    Socket was closed
     * @return               The accepted socket
     * @throws  SystemError  Couldn't accept the connection
     * @cancellationpoint    Yes
     */
    TcpSock::Impl* accept() {
        const int fd = ::accept(sd, nullptr, nullptr);
        Shield    shield{};

        if (fd == -1) {
            {
                Guard guard{mutex};
                if (shutdownCalled)
                    return nullptr;
            }

            throw SYSTEM_ERROR("accept() failure on socket " + to_string());
        }

        return new TcpSock::Impl(fd);
    }
};

TcpSrvrSock::TcpSrvrSock(
        const SockAddr sockAddr,
        const int      queueSize)
    : TcpSock(new Impl(sockAddr, queueSize)) {
}

void TcpSrvrSock::listen(const int size) const {
    static_cast<TcpSrvrSock::Impl*>(pImpl.get())->listen(size);
}

TcpSock TcpSrvrSock::accept() const {
    return TcpSock{static_cast<TcpSrvrSock::Impl*>(pImpl.get())->accept()};
}

/******************************************************************************/

/// A client-side TCP socket
class TcpClntSock::Impl final : public TcpSock::Impl
{
    /**
     * Connects this socket to the remote address
     *
     * @throw RuntimeError  Couldn't connect to remote address at this time
     * @throw SystemError   System failure
     */
    void connect() const {
        struct sockaddr_storage storage;
        int                     status = ::connect(sd, rmtSockAddr.get_sockaddr(storage),
                sizeof(storage));
        if (errno == EADDRNOTAVAIL || errno == ECONNREFUSED || errno == ENETUNREACH ||
                errno == ETIMEDOUT || errno == ECONNRESET || errno == EHOSTUNREACH ||
                errno == ENETDOWN)
            throw RUNTIME_ERROR("Couldn't connect socket " + to_string() + ": " +
                    ::strerror(errno));

        if (status)
            throw SYSTEM_ERROR("connect() failure");

        LOG_DEBUG("Connected socket %s", to_string().data());
    }

public:
    /**
     * Constructs.
     * @param[in] family  The address family (e.g., AF_INET, AF_INET6)
     */
    Impl(const int family)
        : TcpSock::Impl(family, true)
    {}

    /**
     * Constructs. Attempts to connect to a remote server.
     *
     * @param[in] srvrAddr         Address of remote server
     * @throw     LogicError       Destination port number is zero
     * @throw     RuntimeError     Couldn't connect to remote server at this time
     * @throw     SystemError      System failure
     * @exceptionsafety            Strong guarantee
     * @cancellationpoint          Yes
     */
    Impl(const SockAddr srvrAddr)
        : TcpSock::Impl(srvrAddr.getInetAddr())
    {
        //LOG_DEBUG("Checking port number");
        if (srvrAddr.getPort() == 0)
            throw LOGIC_ERROR("Port number of " + srvrAddr.to_string() + " is " "zero");

        //LOG_DEBUG("Setting remote socket address");
        rmtSockAddr = srvrAddr;

        //LOG_DEBUG("Connecting socket");
        connect();
    }
};

TcpClntSock::TcpClntSock(const int family)
    : TcpSock(new Impl(family))
{}

TcpClntSock::TcpClntSock(const SockAddr srvrAddr)
    : TcpSock(new Impl(srvrAddr))
{}

/******************************************************************************/

/// Implementation of a UDP socket
class UdpSock::Impl final : public Socket::Impl
{
    char          buf[MAX_PAYLOAD]; ///< UDP payload buffer
    size_t        bufLen;           ///< Number of read bytes in buffer
    struct pollfd pollfd;           ///< poll(2) structure

    /**
     * Joins a UDP socket to a source-specific multicast group.
     *
     * @param[in] sd       UDP socket identifier
     * @param[in] ssmAddr  Socket address of source-specific multicast group
     * @param[in] srcAddr  IP address of source host
     * @param[in] ifAddr   IP address of interface to use. If wildcard, then O/S chooses.
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  Maybe (`::getaddrinfo()` may be one and will be
     *                     called if either address is based on a name)
     */
    static void join(
            const int      sd,
            const SockAddr ssmAddr,
            const InetAddr srcAddr,
            const InetAddr ifAddr)
    {
        // NB: The following is independent of protocol (i.e., IPv4 or IPv6)

        struct group_source_req mreq = {};
        mreq.gsr_interface = ifAddr.getIfaceIndex();
        ssmAddr.getInetAddr().get_sockaddr(mreq.gsr_group, ssmAddr.getPort());
        srcAddr.get_sockaddr(mreq.gsr_source, 0);

        if (::setsockopt(sd, IPPROTO_IP, MCAST_JOIN_SOURCE_GROUP, &mreq, sizeof(mreq)))
            throw SYSTEM_ERROR("Couldn't join socket " + std::to_string(sd) + " to multicast group "
                    + ssmAddr.to_string() + " from source " + srcAddr.to_string() + "on interface "
                    + ifAddr.to_string());
        LOG_DEBUG("Joined socket %d to multicast group %s from source %s on interface %u", sd,
                ssmAddr.to_string().data(), srcAddr.to_string().data(), mreq.gsr_interface);
    }

    /**
     * Reads the next UDP packet from the socket into the buffer.
     *
     * @retval    false         EOF or `halt()` called
     * @retval    true          Success
     * @throws    SystemError   I/O error
     * @throws    RuntimeError  Packet is too small
     * @cancellationpoint       Yes
     */
    bool readPacket() {
        // poll(2) is used so `shutdown()` works
        int status = ::poll(&pollfd, 1, -1); // -1 => indefinite wait

        if (shutdownCalled || (pollfd.revents & POLLHUP))
            return false;
        if (pollfd.revents & (POLLERR | POLLNVAL))
            return false;
        if (status == -1)
            throw SYSTEM_ERROR("::poll() failure on socket " + to_string());

        const auto nbytes = ::recv(sd, buf, MAX_PAYLOAD, 0);

        if (nbytes < 0)
            throw SYSTEM_ERROR("Couldn't read from socket" + to_string());

        bufLen = nbytes;
        bytesRead = 0;

        return true;
    }

protected:
    bool writeBytes(const void*  data,
                    const size_t nbytes) override {
        if (bytesWritten + nbytes > MAX_PAYLOAD)
            throw LOGIC_ERROR("Maximum UDP payload is " +
                    std::to_string(MAX_PAYLOAD) + " bytes; not " +
                    std::to_string(bytesWritten+nbytes));

        ::memcpy(buf + bytesWritten, data, nbytes);
        return true;
    }

    bool readBytes(void*        data,
                   const size_t nbytes) override {
        while (bufLen == 0)
            if (!readPacket())
                return false;

        const auto nleft = bufLen - bytesRead;
        if (nbytes > nleft)
            throw LOGIC_ERROR(std::to_string(nleft) +
                    " bytes left in buffer; not " +  std::to_string(nbytes));

        ::memcpy(data, buf + bytesRead, nbytes);

        return true;
    }

public:
    /**
     * Constructs a sending UDP socket. If the destination IP address of the socket is a multicast
     * group, then the time-to-live is set and loopback of datagrams is enabled.
     *
     * @param[in] destAddr     Destination socket address
     * @param[in] ifaceAddr    IP address of interface to use. If wildcard, then O/S will choose.
     * @throw     LogicError   Socket can't use given interface
     * @cancellationpoint
     */
    Impl(   const SockAddr destAddr,
            const InetAddr ifaceAddr)
        : Socket::Impl(destAddr.getInetAddr(), SOCK_DGRAM, IPPROTO_UDP)
        , buf()
        , bufLen(0)
    {
        if (destAddr.getInetAddr().isMulticast()) {
            unsigned char  ttl = 250;

            if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)))
                throw SYSTEM_ERROR("Couldn't set time-to-live for multicast packets");

            // Enable loopback of multicast datagrams
            {
                unsigned char enable = 1;
                if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &enable, sizeof(enable)))
                    throw SYSTEM_ERROR("Couldn't enable local loopback of multicast datagrams");
            }
        }

        rmtSockAddr = destAddr;

        // Must precede `connect()` if interface address isn't wildcard
        setMcastIface(ifaceAddr);

        //destAddr.connect(sd);
        //LOG_DEBUG("Connecting socket %d to %s", sd, destAddr.to_string().data());
        struct sockaddr_storage storage;
        if (::connect(sd, destAddr.get_sockaddr(storage), sizeof(storage)))
            throw SYSTEM_ERROR("connect() failure");
    }

    /**
     * Constructs a sending UDP socket. The operating system will choose which interface to use. If
     * the destination IP address of the socket is a multicast group, then the time-to-live is set
     * and loopback of datagrams is enabled.
     *
     * @param[in] destAddr   Destination socket address
     * @cancellationpoint
     */
    Impl(const SockAddr destAddr)
        : Impl(destAddr, destAddr.getInetAddr().getWildcard())
    {}

    /**
     * Constructs a receiving, source-specific multicast, UDP socket.
     *
     * @param[in] ssmAddr     Socket address of source-specific multicast group
     * @param[in] srcAddr     IP address of source host
     * @param[in] iface       IP address of interface to use. If wildcard, then O/S chooses.
     * @throw     LogicError  IP address families don't match
     * @cancellationpoint     Yes
     */
    Impl(   const SockAddr ssmAddr,
            const InetAddr srcAddr,
            const InetAddr iface)
        : Socket::Impl(ssmAddr.getInetAddr(), SOCK_DGRAM, IPPROTO_UDP)
        , buf()
        , bufLen(0)
    {
        if (ssmAddr.getInetAddr().getFamily() != srcAddr.getFamily() ||
                srcAddr.getFamily() != iface.getFamily())
            throw LOGIC_ERROR("IP address families don't match: ssmAddr=" +
                    std::to_string(ssmAddr.getInetAddr().getFamily()) + ", srcAddr=" +
                    std::to_string(srcAddr.getFamily()) + ", iface=" +
                    std::to_string(iface.getFamily()));

        auto& ipAddr = ssmAddr.getInetAddr();
        if (!ipAddr.isSsm())
            throw INVALID_ARGUMENT("Multicast group IP address, " + ipAddr.to_string() +
                    ", isn't source-specific");

        pollfd.fd = sd;
        pollfd.events = POLLIN;

        // Allow multiple sockets to receive the same source-specific multicast
        const int yes = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0)
            throw SYSTEM_ERROR("Couldn't reuse the same source-specific multicast address");
        // Allow the local host to receive the same source-specific multicast
        const unsigned char flag = 1;
        if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &flag, sizeof(flag)))
            throw SYSTEM_ERROR("Couldn't enable local loopback of multicast on socket " +
                    std::to_string(sd));

        ssmAddr.bind(sd);
        join(sd, ssmAddr, srcAddr, iface);
    }

    /**
     * Sets the interface to be used by the UDP socket for multicasting. The
     * default is system dependent.
     *
     * @param[in] iface        IP address of interface
     * @throw     LogicError   Socket can't use given interface
     */
    void setMcastIface(const InetAddr iface) const {
        try {
            iface.makeIface(sd);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(LOGIC_ERROR("Socket " + to_string() + " can't use interface " +
                    iface.to_string()));
        }
    }

    std::string to_string() const override {
        return "{sd=" + std::to_string(sd) + ", lcl=" + getLclAddr().to_string()
                + ", proto=UDP, rmt=" + getRmtAddr().to_string() + "}";
    }

    bool flush() {
        const auto nbytes = ::write(sd, buf, bytesWritten);

        if (nbytes == -1)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(bytesWritten)
                    + " bytes to socket " + to_string());
        if (nbytes == 0)
            return false; // Lost connection

        bytesWritten = 0;
        return true;
    }

    void clear() {
        bufLen = bytesRead = bytesWritten = 0;
    }
};

UdpSock::UdpSock(const SockAddr destAddr)
    : Socket(new Impl{destAddr})
{}

UdpSock::UdpSock(
        const SockAddr destAddr,
        const InetAddr ifaceAddr)
    : Socket(new Impl{destAddr, ifaceAddr})
{}

UdpSock::UdpSock(
        const SockAddr ssmAddr,
        const InetAddr srcAddr,
        const InetAddr iface)
    : Socket(new Impl{ssmAddr, srcAddr, iface})
{}

const UdpSock& UdpSock::setMcastIface(const InetAddr iface) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->setMcastIface(iface);
    return *this;
}

} // namespace

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

#include "InetAddr.h"
#include "Socket.h"

#include "error.h"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <limits.h>
#include <mutex>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

namespace hycast {

class Socket::Impl
{
protected:
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;

    mutable Mutex mutex;
    SockAddr      rmtSockAddr;
    int           sd;             ///< Socket descriptor
    bool          shutdownCalled; ///< `shutdown()` has been called?

    explicit Impl(const int sd) noexcept
        : mutex()
        , rmtSockAddr()
        , sd{sd}
        , shutdownCalled{false}
    {
        if (sd < 0)
            throw INVALID_ARGUMENT("Socket descriptor is " +
                    std::to_string(sd));

        struct sockaddr_storage storage = {};
        socklen_t               socklen = sizeof(storage);

        if (::getpeername(sd, reinterpret_cast<struct sockaddr*>(&storage),
                &socklen) == 0)
            rmtSockAddr = SockAddr(storage);
    }

    /**
     * Idempotent.
     *
     * @pre                `mutex` is locked
     * @param[in] what     What to shut down. One of `SHUT_RD`, `SHUT_WR`, or
     *                     `SHUT_RDWR`.
     * @throw SystemError  `::shutdown()` failure
     */
    void shut(const int what) {
        if (::shutdown(sd, what) && errno != ENOTCONN)
            throw SYSTEM_ERROR("::shutdown failure on socket " +
                    std::to_string(sd));
        shutdownCalled = true;
    }

public:
    virtual ~Impl() noexcept {
        ::close(sd);
    }

    size_t hash() const noexcept {
        return getLclAddr().hash() ^ getRmtAddr().hash();
    }

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

    operator bool() const {
        Guard guard{mutex};
        return !shutdownCalled && sd >= 0;
    }

    SockAddr getLclAddr() const {
        struct sockaddr_storage storage = {};
        socklen_t               socklen = sizeof(storage);

        if (::getsockname(sd, reinterpret_cast<struct sockaddr*>(&storage),
                &socklen))
            throw SYSTEM_ERROR("getsockname() failure on socket " +
                    std::to_string(sd));

        return SockAddr(storage);
    }

    SockAddr getRmtAddr() const noexcept {
        return rmtSockAddr;
    }

    in_port_t getLclPort() const {
        return getLclAddr().getPort();
    }

    in_port_t getRmtPort() const noexcept {
        return getRmtAddr().getPort();
    }

    /**
     * Shuts down the socket. If reading is shut down, then `accept()` will
     * return `nullptr` and `read()` will return false. Idempotent.
     *
     * @param what          What to shut down. One of `SHUT_RD`, `SHUT_WR`, or
     *                      `SHUT_RDWR`
     * @throws SystemError  `::shutdown()` failure
     */
    void shutdown(const int what) {
        Guard guard{mutex};
        shut(what);
    }

    bool isShutdown() const {
        Guard guard{mutex};
        return shutdownCalled;
    }
};

Socket::Socket(Impl* impl)
    : pImpl{impl}
{}

Socket::~Socket()
{}

Socket::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

size_t Socket::hash() const noexcept {
    return pImpl ? pImpl->hash() : 0;
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

SockAddr Socket::getLclAddr() const
{
    SockAddr sockAddr(pImpl->getLclAddr());
    //LOG_DEBUG("%s", sockAddr.to_string().c_str());
    return sockAddr;
}

in_port_t Socket::getLclPort() const
{
    return pImpl->getLclPort();
}

SockAddr Socket::getRmtAddr() const
{
    return pImpl->getRmtAddr();
}

in_port_t Socket::getRmtPort() const
{
    return pImpl->getRmtPort();
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

class InetSock::Impl : public Socket::Impl
{
protected:
    /**
     * Constructs.
     *
     * @param[in] sd       Socket descriptor
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint
     */
    Impl(const int sd)
        : Socket::Impl(sd)
    {}

public:
    virtual ~Impl() noexcept
    {}
};

InetSock::InetSock(Impl* impl)
    : Socket{impl}
{}

InetSock::~InetSock()
{}

/******************************************************************************/

class TcpSock::Impl : public InetSock::Impl
{
    typedef uint64_t MaxType;

    mutable unsigned      bytesWritten;
    mutable unsigned      bytesRead;

protected:
    friend class TcpSrvrSock;

    Impl();

    /**
     * Constructs.
     *
     * @param[in] sd       Socket descriptor
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint
     */
    Impl(const int sd)
        : InetSock::Impl{sd}
        , bytesWritten{0}
        , bytesRead{0}
    {}

    inline size_t padLen(
            const unsigned nbytes,
            const size_t   align)
    {
        return (align - nbytes) % align;
    }

    inline bool alignWriteTo(size_t nbytes)
    {
        static MaxType pad;
        return write(&pad, padLen(bytesWritten, nbytes));
    }

    inline bool alignReadTo(size_t nbytes)
    {
        static MaxType pad;
        read(&pad, padLen(bytesRead, nbytes));
    }

public:
    std::string to_string() const
    {
        return "{rmt: " + getRmtAddr().to_string() + ", lcl: " +
                getLclAddr().to_string() + "}";
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
            throw SYSTEM_ERROR("Couldn't set TCP_NODELAY to %d on socket " +
                    std::to_string(enable) + std::to_string(sd));
        }
    }

    /**
     * Writes to the socket. No host-to-network translation is performed.
     *
     * @param[in] data         Bytes to write
     * @param[in] nbytes       Number of bytes to write
     * @retval    `false`      Connection is closed
     * @retval    `true`       Success
     * @throws    SystemError  System error
     */
    bool write(const void* data,
               size_t      nbytes) const
    {
        LOG_TRACE;
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
            LOG_TRACE;
            if (::poll(&pollfd, 1, -1) == -1)
                throw SYSTEM_ERROR("poll() failure for host " +
                        getRmtAddr().to_string());
            LOG_TRACE;
            if (pollfd.revents & POLLHUP)
                return false;
            LOG_TRACE;
            if (pollfd.revents & (POLLOUT | POLLERR)) {
                // Cancellation point
                LOG_DEBUG("sd=%d, bytes=%p, nbytes=%zu", sd, bytes, nbytes);
                auto nwritten = ::write(sd, bytes, nbytes);
                LOG_DEBUG("nwritten=%zd", nwritten);

                if (nwritten == -1) {
                    if (errno == ECONNRESET || errno == EPIPE) {
                        LOG_TRACE;
                        return false;
                    }
                    LOG_TRACE;
                    throw SYSTEM_ERROR("write() failure to host "
                            + getRmtAddr().to_string());
                }

                LOG_TRACE;
                nbytes -= nwritten;
                bytes += nwritten;
                bytesWritten += nwritten;
            }
            else {
                LOG_TRACE;
                throw RUNTIME_ERROR("poll() failure for host " +
                        getRmtAddr().to_string());
            }
        }

        return true;
    }

    bool write(std::string str)
    {
        return write(str.size()) && write(str.data(), str.size());
    }

    bool write(const uint8_t value)
    {
        return write(&value, sizeof(value));
    }

    /**
     * Performs network-translation and value-alignment.
     */
    bool write(uint16_t value)
    {
        value = hton(value);
        return alignWriteTo(sizeof(value)) && write(&value, sizeof(value));
    }

    /**
     * Performs network-translation and value-alignment.
     */
    bool write(uint32_t value)
    {
        value = hton(value);
        return alignWriteTo(sizeof(value)) && write(&value, sizeof(value));
    }

    /**
     * Performs network-translation and value-alignment.
     */
    bool write(uint64_t value)
    {
        value = hton(value);
        return alignWriteTo(sizeof(value)) && write(&value, sizeof(value));
    }

    /**
     * Reads from the socket. No network-to-host translation is performed.
     *
     * @param[out] data         Destination buffer
     * @param[in]  nbytes       Number of bytes to read
     * @retval     `true`       Success
     * @retval     `false`      EOF or `shutdown()` called
     * @throw      SystemError  I/O failure
     */
    bool read(void* const  data,
              const size_t nbytes) const
    {
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
            if (::poll(&pollfd, 1, -1) == -1)
                throw SYSTEM_ERROR("poll() failure for host " +
                        getRmtAddr().to_string());
            if (pollfd.revents & POLLHUP)
                return false; // EOF
            if (pollfd.revents & (POLLIN | POLLERR)) {
                auto nread = ::read(sd, bytes, nleft);

                if (nread == -1)
                    throw SYSTEM_ERROR("Couldn't read from connection with "
                            "host " + getRmtAddr().to_string());
                if (nread == 0)
                    return false; // EOF

                nleft -= nread;
                bytes += nread;
                bytesRead += nread;
            }
            else {
                throw RUNTIME_ERROR("poll() failure for host " +
                        getRmtAddr().to_string());
            }
        }

        return true;
    }

    /**
     * Reads a string from the socket.
     *
     * @param[out] str      String to be read
     * @retval     `true`   Success
     * @retval     `false`  EOF
     */
    bool read(std::string& str)
    {
        bool                   success = false;
        std::string::size_type size;

        if (read(size)) {
            char bytes[size];
            if (read(bytes, size)) {
                str.assign(bytes, size);
                success = true;
            }
        }

        return success;
    }

    /**
     * @param[out] value
     * @retval     `true`   Success
     * @retval     `false`  EOF
     */
    bool read(uint8_t& value)
    {
        return read(&value, sizeof(value));
    }

    /**
     * Performs network-translation and value-alignment.
     *
     * @param[out] value
     * @retval     `true`   Success
     * @retval     `false`  EOF
     */
    bool read(uint16_t& value)
    {
        alignReadTo(sizeof(value));
        if (!read(&value, sizeof(value)))
            return false;
        value = ntoh(value);
        return true;
    }

    /**
     * Performs network-translation and value-alignment.
     * @param[out] value
     * @retval     `true`   Success
     * @retval     `false`  EOF
     */
    bool read(uint32_t& value)
    {
        alignReadTo(sizeof(value));
        if (!read(&value, sizeof(value)))
            return false;
        value = ntoh(value);
        return true;
    }

    /**
     * Performs network-translation and value-alignment.
     * @param[out] value
     * @retval     `true`   Success
     * @retval     `false`  EOF
     */
    bool read(uint64_t& value)
    {
        alignReadTo(sizeof(value));
        if (!read(&value, sizeof(value)))
            return false;
        value = ntoh(value);
        return true;
    }
};

TcpSock::TcpSock(Impl* impl)
    : InetSock{impl}
{}

TcpSock::~TcpSock()
{}

std::string TcpSock::to_string() const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->to_string();
}

TcpSock& TcpSock::setDelay(bool enable)
{
    static_cast<TcpSock::Impl*>(pImpl.get())->setDelay(enable);
    return *this;
}

bool TcpSock::write(
        const void* bytes,
        size_t      nbytes) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->write(bytes, nbytes);
}

bool TcpSock::write(const std::string str) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->write(str);
}

bool TcpSock::write(const bool value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->write(static_cast<uint8_t>(value));
}

bool TcpSock::write(const uint8_t value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->write(value);
}

bool TcpSock::write(const uint16_t value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->write(value);
}

bool TcpSock::write(const uint32_t value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->write(value);
}

bool TcpSock::write(const uint64_t value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->write(value);
}

bool TcpSock::read(void* const  bytes,
                   const size_t nbytes) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(bytes, nbytes);
}

bool TcpSock::read(std::string& str) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(str);
}

bool TcpSock::read(bool& value) const
{
    uint8_t val;
    auto success = static_cast<TcpSock::Impl*>(pImpl.get())->read(val);
    if (success)
        value = val;
    return success;
}

bool TcpSock::read(uint8_t& value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(value);
}

bool TcpSock::read(uint16_t& value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(value);
}

bool TcpSock::read(uint32_t& value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(value);
}

bool TcpSock::read(uint64_t& value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(value);
}

/******************************************************************************/

class TcpSrvrSock::Impl final : public TcpSock::Impl
{
public:
    /**
     * Constructs. Calls `::listen()`.
     *
     * @param[in] sockAddr           Server's local socket address
     * @param[in] queueSize          Size of listening queue
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     * @throws    std::system_error  `::listen()` failure
     */
    Impl(   const SockAddr& sockAddr,
            const int       queueSize)
        : TcpSock::Impl{sockAddr.socket(SOCK_STREAM)}
    {
        const int enable = 1;

        //LOG_DEBUG("Setting SO_REUSEADDR");
        if (::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_REUSEADDR on socket " +
                    std::to_string(sd) + ", address " + sockAddr.to_string());

        //LOG_DEBUG("Binding socket");
        sockAddr.bind(sd);

        //LOG_DEBUG("Setting SO_KEEPALIVE");
        if (::setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_KEEPALIVE on socket " +
                    std::to_string(sd) + ", address " + sockAddr.to_string());

        if (::listen(sd, queueSize))
            throw SYSTEM_ERROR("listen() failure: {sock: " + std::to_string(sd)
                    + ", queueSize: " + std::to_string(queueSize) + "}");
    }

    std::string to_string() const
    {
        return getLclAddr().to_string();
    }

    /**
     * Accepts an incoming connection. Calls `::accept()`.
     *
     * @retval  `nullptr`          Socket was closed
     * @return                     The accepted socket
     * @throws  std::system_error  `::accept()` failure
     * @cancellationpoint          Yes
     */
    TcpSock::Impl* accept()
    {
        const int fd = ::accept(sd, nullptr, nullptr);

        if (fd == -1) {
            {
                Guard guard{mutex};
                if (shutdownCalled)
                    return nullptr;
            }

            throw SYSTEM_ERROR("accept() failure on socket " +
                        std::to_string(sd));
        }

        return new TcpSock::Impl(fd);
    }
};

TcpSrvrSock::TcpSrvrSock(
        const SockAddr& sockAddr,
        const int       queueSize)
    : TcpSock{new Impl(sockAddr, queueSize)}
{}

std::string TcpSrvrSock::to_string() const
{
    return static_cast<Impl*>(pImpl.get())->to_string();
}

TcpSock TcpSrvrSock::accept() const
{
    return TcpSock{static_cast<TcpSrvrSock::Impl*>(pImpl.get())->accept()};
}

/******************************************************************************/

class TcpClntSock::Impl final : public TcpSock::Impl
{
public:
    /**
     * Constructs.
     *
     * @param[in] sockAddr    Address of remote endpoint
     * @throw     SystemError Couldn't connect to `sockAddr`
     * @throw     LogicError  Destination port number is zero
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     Yes
     */
    Impl(const SockAddr& sockAddr)
        : TcpSock::Impl(sockAddr.socket(SOCK_STREAM, IPPROTO_TCP))
    {
        if (sockAddr.getPort() == 0)
            throw LOGIC_ERROR("Port number of " + sockAddr.to_string() + " is "
                    "zero");

        sockAddr.connect(sd);
    }
};

TcpClntSock::TcpClntSock(const SockAddr& sockAddr)
    : TcpSock(new Impl(sockAddr))
{}

/******************************************************************************/

class UdpSock::Impl final : public InetSock::Impl
{
    typedef uint64_t MaxType;
    typedef void   (*Ntoh)(void*);

    uint8_t       uint8s[MAX_PAYLOAD];    ///< Write buffer for 1-byte values
    uint16_t      uint16s[MAX_PAYLOAD/2]; ///< Write buffer for 2-byte values
    uint32_t      uint32s[MAX_PAYLOAD/4]; ///< Write buffer for 4-byte values
    uint64_t      uint64s[MAX_PAYLOAD/8]; ///< Write buffer for 8-byte values
    uint8_t*      nxt8;                   ///< Next 1-byte value to write
    uint16_t*     nxt16;                  ///< Next 2-byte value to write
    uint32_t*     nxt32;                  ///< Next 4-byte value to write
    uint64_t*     nxt64;                  ///< Next 8-byte value to write
    struct iovec  writeIov[IOV_MAX] = {}; ///< Write I/O vector
    int           writeIovCnt;            ///< Write I/O vector size
    size_t        numWrite;               ///< Current number of bytes to write
    struct iovec  readIov[IOV_MAX];       ///< Read I/O vector
    uint64_t      skipBuf[MAX_PAYLOAD];   ///< Buffer for skipping bytes
    struct msghdr msghdr = {};            ///< UDP `::rcvmsg()` structure
    Ntoh          ntohs[IOV_MAX] = {};    ///< Network-to-host converters
    size_t        numPeek;                ///< Current number of bytes to peek
    struct pollfd pollfd;                 ///< poll(2) structure

    /**
     * Vets adding an additional I/O vector element.
     *
     * @param[in] nbytes           Proposed total number of bytes
     * @param[in] iovlen           Current I/O vector length
     * @throws    InvalidArgument  Addition would exceed UDP packet size
     * @throws    LogicError       Out of vector I/O elements
     */
    void vetIoElt(
            const size_t nbytes,
            const size_t iovlen)
    {
        if (nbytes > sizeof(uint8s))
            throw INVALID_ARGUMENT(std::to_string(nbytes) + "-byte "
                    "UDP packet isn't supported");

        if (iovlen == IOV_MAX)
            throw LOGIC_ERROR(std::to_string(IOV_MAX+1) + "-element "
                    "I/O vector isn't supported");
    }

    /**
     * Returns the number of padding bytes necessary to align the next I/O to
     * a given amount.
     *
     * @param[in] nbytes  Total number of previous bytes
     * @param[in] align   Alignment requirement in bytes
     * @return            Number of padding bytes necessary
     */
    inline size_t padLen(
            const unsigned nbytes,
            const size_t   align)
    {
        return (align - nbytes) % align;
    }

    /**
     * Adds a write I/O element.
     *
     * @param[in] data             Bytes to be added. Must exist until `write()`
     *                             returns.
     * @param[in] nbytes           Number of bytes
     * @throws    InvalidArgument  Addition would exceed UDP packet size
     * @throws    LogicError       Out of vector I/O elements
     */
    void addWriteElt(
            const void* const data,
            const size_t      nbytes)
    {
        vetIoElt(numWrite + nbytes, writeIovCnt);
        numWrite += nbytes;
        writeIov[writeIovCnt].iov_base = const_cast<void*>(data);
        writeIov[writeIovCnt++].iov_len = nbytes;
    }

    /**
     * Aligns the next write addition.
     *
     * @param[in] nbytes  Alignment requirement in bytes
     */
    void alignWriteTo(const size_t nbytes)
    {
        static MaxType pad;
        const size_t   len = padLen(numWrite, nbytes);

        if (len)
            addWriteElt(&pad, len);
    }

    /**
     * Adds a peek I/O element.
     *
     * @param[out] data    Destination for bytes to be peeked. Must exist until
     *                     `peek()` or `discard()` is called.
     * @param[in]  nbytes  Number of bytes
     * @param[in]  ntoh    Network-to-host converter or `nullptr`
     */
    void addPeekElt(
            void* const  data,
            const size_t nbytes,
            Ntoh         ntoh = nullptr)
    {
        vetIoElt(readIov[0].iov_len + numPeek + nbytes, msghdr.msg_iovlen);
        numPeek += nbytes;
        readIov[msghdr.msg_iovlen].iov_base = data;
        readIov[msghdr.msg_iovlen].iov_len = nbytes;
        ntohs[msghdr.msg_iovlen++] = ntoh;
    }

    /**
     * Aligns the next peek.
     *
     * @param[in] nbytes  Alignment requirement in bytes.
     */
    void alignPeekTo(const size_t nbytes)
    {
        auto numRead = readIov[0].iov_len + numPeek;
        auto numPadBytes = padLen(numRead, nbytes);

        if (numPadBytes)
            addPeekElt(skipBuf, numPadBytes);
    }

    static void cvt16(void* value)
    {
        *static_cast<uint16_t*>(value) = ntoh(*static_cast<uint16_t*>(value));
    }

    static void cvt32(void* value)
    {
        *static_cast<uint32_t*>(value) = ntoh(*static_cast<uint32_t*>(value));
    }

    static void cvt64(void* value)
    {
        *static_cast<uint64_t*>(value) = ntoh(*static_cast<uint64_t*>(value));
    }

    /**
     * Constructs.
     *
     * @cancellationpoint
     */
    explicit Impl(const int sd)
        : InetSock::Impl(sd)
        , nxt8{uint8s}
        , nxt16{uint16s}
        , nxt32{uint32s}
        , nxt64{uint64s}
        , writeIovCnt{0}
        , numWrite{0}
        , readIov{}
        , numPeek{0}
    {
        readIov[0].iov_base = skipBuf;
        msghdr.msg_name = msghdr.msg_control = nullptr;
        msghdr.msg_iov = readIov;
        msghdr.msg_iovlen = 1;
    }

    /**
     * Resets peeking parameters after a call to `peek()` or `discard()`.
     *
     * @param numToSkip  Number of previously-peeked bytes to skip
     */
    void resetForNextPeek(const size_t numToSkip)
    {
        numPeek = 0;
        msghdr.msg_iovlen = 1;
        readIov[0].iov_len = numToSkip;
    }

public:
    /**
     * Constructs a sending UDP socket.
     *
     * @cancellationpoint
     */
    Impl(const SockAddr& grpAddr)
        : Impl(grpAddr.socket(SOCK_DGRAM, IPPROTO_UDP))
    {
        unsigned char  ttl = 250; // Source-specific multicast => large value OK

        if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)))
            throw SYSTEM_ERROR(
                    "Couldn't set time-to-live for multicast packets");

        // Enable loopback of multicast datagrams
        {
            unsigned char enable = 1;
            if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &enable,
                    sizeof(enable)))
                throw SYSTEM_ERROR(
                        "Couldn't enable loopback of multicast datagrams");
        }

        grpAddr.connect(sd);
    }

    /**
     * Constructs a source-specific receiving UDP socket.
     *
     * @cancellationpoint  Yes
     */
    Impl(   const SockAddr& grpAddr,
            const InetAddr& rmtAddr)
        : Impl(grpAddr.socket(SOCK_DGRAM, IPPROTO_UDP))
    {
        pollfd.fd = sd;
        pollfd.events = POLLIN;
        grpAddr.bind(sd);
        grpAddr.join(sd, rmtAddr);
    }

    /**
     * Sets the interface to be used by the UDP socket for multicasting. The
     * default is system dependent.
     */
    void setMcastIface(const InetAddr& iface) const
    {
        iface.setMcastIface(sd);
    }

    std::string to_string() const
    {
        return "{rmt: " + getRmtAddr().to_string() + ", lcl: " +
                getLclAddr().to_string() + "}";
    }

    /**
     * Adds bytes to be written.
     *
     * @param[in] data    Bytes to be added. Must exist until `send()` returns.
     * @param[in] nbytes  Number of bytes to add.
     */
    void addWrite(
            const void* const data,
            const size_t      nbytes)
    {
        addWriteElt(data, nbytes);
    }

    void addWrite(const uint8_t value)
    {
        *nxt8 = value;
        addWrite(nxt8++, sizeof(value));
    }

    void addWrite(const bool value)
    {
        addWrite(static_cast<uint8_t>(value));
    }

    void addWrite(const uint16_t value)
    {
        alignWriteTo(sizeof(value));
        *nxt16 = hton(value);
        addWrite(nxt16++, sizeof(value));
    }

    void addWrite(const uint32_t value)
    {
        alignWriteTo(sizeof(value));
        *nxt32 = hton(value);
        addWrite(nxt32++, sizeof(value));
    }

    void addWrite(const uint64_t value)
    {
        alignWriteTo(sizeof(value));
        *nxt64 = hton(value);
        addWrite(nxt64++, sizeof(value));
    }

    void addWrite(const std::string& string)
    {
        addWrite(string.size());
        addWrite(string.data(), string.size());
    }

    void write()
    {
        if (::writev(sd, writeIov, writeIovCnt) == -1)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(writeIovCnt) +
                    "-element I/O vector to host " + getRmtAddr().to_string());
        numWrite = 0;
        writeIovCnt = 0;
    }

    /**
     * Adds a peek at a UDP packet. The packet is not read. Previously peeked
     * bytes and previously added bytes are skipped.
     *
     * @param[out data             Destination for peeked bytes
     * @param[in] nbytes           Number of bytes to peek
     * @param[in] cvt              Network to host converter
     * @retval    `false`          EOF or `halt()` called
     * @retval    `true`           Success
     * @throws    InvalidArgument  Addition would exceed UDP packet size
     * @throws    LogicError       Out of vector I/O elements
     * @cancellationpoint          No
     */
    void addPeek(
            void* const  data,
            const size_t nbytes,
            Ntoh         ntoh = nullptr)
    {
        addPeekElt(data, nbytes, ntoh);
    }

    void addPeek(uint8_t& value)
    {
        addPeek(&value, sizeof(value));
    }

    void addPeek(uint16_t& value)
    {
        alignPeekTo(sizeof(value));
        addPeek(&value, sizeof(value), &cvt16);
    }

    void addPeek(uint32_t& value)
    {
        alignPeekTo(sizeof(value));
        addPeek(&value, sizeof(value), &cvt32);
    }

    void addPeek(uint64_t& value)
    {
        alignPeekTo(sizeof(value));
        addPeek(&value, sizeof(value), &cvt64);
    }

    void discard()
    {
        char byte;
        ::read(sd, &byte, sizeof(byte)); // Discards entire packet
        resetForNextPeek(0);
    }

    /**
     * Peeks at the UDP packet using the I/O vector set by previous calls to
     * `setPeek()`. Previously peeked bytes are skipped.
     *
     * @retval    `false`       EOF or `halt()` called
     * @retval    `true`        Success
     * @throws    SystemError   I/O error
     * @throws    RuntimeError  Packet is too small
     * @cancellationpoint       Yes
     */
    bool peek()
    {
        // poll(2) is used so `shutdown()` works
        int status = ::poll(&pollfd, 1, -1); // -1 => indefinite wait

        if (shutdownCalled || (pollfd.revents & POLLHUP))
            return false;

        if (status == -1)
            throw SYSTEM_ERROR("::poll() failure on socket" +
                    std::to_string(sd));

        if (pollfd.revents & (POLLERR | POLLNVAL))
            return false;

        // Peek packet
        //LOG_DEBUG("Skipping %zu bytes; peeking at %zu", numPeeked, reqNumPeek);
        const ssize_t nread = ::recvmsg(sd, &msghdr, MSG_PEEK);
        //LOG_DEBUG("Read %zd bytes", nread);

        if (nread < 0)
            throw SYSTEM_ERROR("Couldn't read from socket" +
                    std::to_string(sd));

        if (nread != readIov[0].iov_len + numPeek) {
            discard();
            return false; // EOF
        }

        // Perform network-to-host translation
        for (int i = 1; i < msghdr.msg_iovlen; ++i) // `0` is for skipped bytes
            if (ntohs[i])
                ntohs[i](readIov[i].iov_base);

        resetForNextPeek(nread);

        return true;
    }
};

UdpSock::UdpSock(const SockAddr& grpAddr)
    : InetSock{new Impl(grpAddr)}
{}

UdpSock::UdpSock(
        const SockAddr& grpAddr,
        const InetAddr& rmtAddr)
    : InetSock{new Impl(grpAddr, rmtAddr)}
{}

const UdpSock& UdpSock::setMcastIface(const InetAddr& iface) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->setMcastIface(iface);
    return *this;
}

std::string UdpSock::to_string() const
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->to_string();
}

void UdpSock::addWrite(
        const void* const data,
        const size_t      nbytes) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->addWrite(data, nbytes);
}

void UdpSock::addWrite(const uint8_t value) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->addWrite(value);
}

void UdpSock::addWrite(const bool value) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->addWrite(value);
}

void UdpSock::addWrite(const uint16_t value) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->addWrite(value);
}

void UdpSock::addWrite(const uint32_t value) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->addWrite(value);
}

void UdpSock::addWrite(const uint64_t value) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->addWrite(value);
}

void UdpSock::addWrite(const std::string& string) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->addWrite(string);
}

void UdpSock::write() const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->write();
}

void UdpSock::addPeek(
        void* const  data,
        const size_t nbytes)
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->addPeek(data, nbytes);
}

void UdpSock::addPeek(uint8_t& value)
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->addPeek(value);
}

void UdpSock::addPeek(uint16_t& value)
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->addPeek(value);
}

void UdpSock::addPeek(uint32_t& value)
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->addPeek(value);
}

void UdpSock::addPeek(uint64_t& value)
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->addPeek(value);
}

void UdpSock::discard()
{
    static_cast<UdpSock::Impl*>(pImpl.get())->discard();
}

bool UdpSock::peek() const
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->peek();
}

} // namespace

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

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <inttypes.h>
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
    Impl()
        : mutex()
        , rmtSockAddr()
        , sd{-1}
        , shutdownCalled{false}
    {}

    /**
     * @param[in] sd  Socket descriptor
     */
    void init(const int sd)
    {
        LOG_DEBUG("Initializing socket descriptor %d", sd);
        this->sd = sd;

        struct sockaddr_storage storage = {};
        socklen_t               socklen = sizeof(storage);

        if (::getpeername(sd, reinterpret_cast<struct sockaddr*>(&storage),
                &socklen) == 0)
            rmtSockAddr = SockAddr(storage);
    }

protected:
    using Mutex   = std::mutex;
    using Guard   = std::lock_guard<Mutex>;

    mutable Mutex         mutex;
    SockAddr              rmtSockAddr;
    int                   sd;             ///< Socket descriptor
    bool                  shutdownCalled; ///< `shutdown()` has been called?
    mutable unsigned      bytesWritten =0;
    mutable unsigned      bytesRead =0;
    static const uint64_t writePad;       ///< Write alignment buffer
    static uint64_t       readPad;        ///< Read alignment buffer

    /**
     * Constructs a server-side socket. Closes the socket descriptor on
     * destruction.
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
     * Constructs a client-side socket.
     *
     * @param[in] sockAddr  Socket address
     * @param[in] type      Type of socket (e.g., SOCK_STREAM)
     * @param[in] protocol  Socket protocol (e.g., IPPROTO_TCP)
     */
    Impl(   const SockAddr& sockAddr,
            const int       type,
            const int       protocol) noexcept
        : Impl()
    {
        Shield shield{};
        init(sockAddr.socket(type, protocol));
    }

    static inline size_t padLen(
            const unsigned nbytes,
            const size_t   align) {
        return (align - (nbytes % align)) % align;
        /*
         * Alternative?
         * See <https://en.wikipedia.org/wiki/Data_structure_alignment#Computing_padding>.
         *
         * Works for unsigned and two's-complement `nbytes` but not one's-
         * complement nor sign-magnitude
         *
         * padding = (align - (nbytes & (align - 1))) & (align - 1)
         *         = -nbytes & (align - 1)
         */
    }

    inline bool alignWriteTo(size_t align)
    {
        LOG_DEBUG("bytesWritten=%s, align=%s",
                std::to_string(bytesWritten).data(),
                std::to_string(align).data());
        const auto nbytes = padLen(bytesWritten, align);
        return nbytes
                ? write(&writePad, nbytes)
                : true;
    }

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
     * @retval    `true`    Success
     * @retval    `false`   Lost connection
     */
    virtual bool writeBytes(const void* data,
                            size_t      nbytes) =0;

    /**
     * Reads bytes in an implementation-specific manner. Doesn't modify the
     * number of bytes read.
     *
     * @param[out] data         Destination buffer
     * @param[in]  nbytes       Number of bytes to read
     * @retval     `true`       Success
     * @retval     `false`      Lost connection
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
     * @throw SystemError  `::shutdown()` failure
     */
    void shut(const int what) {
        LOG_DEBUG("Shutting down socket %s", std::to_string(sd).data());
        if (::shutdown(sd, what) && errno != ENOTCONN)
            throw SYSTEM_ERROR("::shutdown failure on socket " +
                    std::to_string(sd));
        shutdownCalled = true;
    }

public:
    virtual ~Impl() noexcept {
        LOG_DEBUG("Closing socket descriptor %d", sd);
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

    virtual std::string to_string() const =0;

    /**
     * Writes bytes.
     *
     * @param[in] data          Bytes to write.
     * @param[in] nbytes        Number of bytes to write.
     * @retval     `true`       Success
     * @retval     `false`      Lost connection
     * @throw      SystemError  I/O failure
     */
    bool write(const void*  data,
               const size_t nbytes) {
        LOG_DEBUG("Writing %zu bytes to %s", nbytes, to_string().data());
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

    virtual bool flush() =0;

    /**
     * Reads bytes.
     *
     * @param[out] data         Destination buffer
     * @param[in]  nbytes       Number of bytes to read
     * @retval     `true`       Success
     * @retval     `false`      Lost connection
     * @throw      SystemError  I/O failure
     */
    bool read(void* const  data,
              const size_t nbytes) {
        if (readBytes(data, nbytes)) {
            bytesRead += nbytes;
            LOG_DEBUG("Read %zu bytes from %s", nbytes, to_string().data());
            return true;
        }
        return false;
    }

    virtual void clear() =0;

    /**
     * Performs value-alignment.
     */
    template<class TYPE>
    bool read(TYPE& value) {
        return alignReadTo(sizeof(value)) && read(&value, sizeof(value));
    }

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

SockAddr Socket::getLclAddr() const {
    SockAddr sockAddr(pImpl->getLclAddr());
    //LOG_DEBUG("%s", sockAddr.to_string().c_str());
    return sockAddr;
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

class TcpSock::Impl : public Socket::Impl
{
protected:
    friend class TcpSrvrSock;

    Impl();

    explicit Impl(const int sd)
        : Socket::Impl(sd)
    {}

    /**
     * Constructs.
     *
     * @param[in] sockAddr  Socket address
     * @exceptionsafety     Strong guarantee
     * @cancellationpoint
     */
    Impl(   const SockAddr& sockAddr)
        : Socket::Impl{sockAddr, SOCK_STREAM, IPPROTO_TCP}
    {}

    /**
     * Writes to the socket. No host-to-network translation is performed.
     *
     * @param[in] data         Bytes to write
     * @param[in] nbytes       Number of bytes to write
     * @retval    `false`      Connection is closed
     * @retval    `true`       Success
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
     * @retval     `true`       Success
     * @retval     `false`      EOF or `shutdown()` called
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
            if (pollfd.revents & POLLHUP)
                return false; // EOF
            if (pollfd.revents & (POLLIN | POLLERR)) {
                auto nread = ::read(sd, bytes, nleft);

                if (nread == -1)
                    throw SYSTEM_ERROR("Couldn't read from socket " +
                            to_string());
                if (nread == 0)
                    return false; // EOF

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

class TcpSrvrSock::Impl final : public TcpSock::Impl
{
public:
    /**
     * Constructs. Calls `::listen()`.
     *
     * @param[in] sockAddr           Server's local socket address
     * @param[in] queueSize          Size of listening queue
     * @throws    std::system_error  `::socket()` failure
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     * @throws    std::system_error  `::listen()` failure
     */
    Impl(   const SockAddr& sockAddr,
            const int       queueSize)
        : TcpSock::Impl{sockAddr}
    {
        //LOG_DEBUG("Setting SO_REUSEADDR");
        const int enable = 1;
        if (::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_REUSEADDR on socket " +
                    to_string());

        //LOG_DEBUG("Binding socket");
        sockAddr.bind(sd);

        //LOG_DEBUG("Setting SO_KEEPALIVE");
        if (::setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_KEEPALIVE on socket " +
                    to_string());

        if (::listen(sd, queueSize))
            throw SYSTEM_ERROR("listen() failure on socket " + to_string() +
                    ", queueSize=" + std::to_string(queueSize));
    }

    std::string to_string() const override
    {
        return "{sd=" + std::to_string(sd) + ", lcl=" + getLclAddr().to_string() + ", proto=TCP}";
    }

    /**
     * Accepts an incoming connection. Calls `::accept()`.
     *
     * @retval  `nullptr`    Socket was closed
     * @return               The accepted socket
     * @throws  SystemError  `::accept()` failure
     * @cancellationpoint    Yes
     */
    TcpSock::Impl* accept()
    {
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
        const SockAddr& sockAddr,
        const int       queueSize)
    : TcpSock(new Impl(sockAddr, queueSize)) {
}

TcpSock TcpSrvrSock::accept() const {
    return TcpSock{static_cast<TcpSrvrSock::Impl*>(pImpl.get())->accept()};
}

/******************************************************************************/

class TcpClntSock::Impl final : public TcpSock::Impl
{
public:
    /**
     * Constructs.
     *
     * @param[in] sockAddr      Address of remote endpoint
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect to `sockAddr`. Bad failure.
     * @throw     RuntimeError  Couldn't connect to `sockAddr`. Might be
     *                          temporary.
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       Yes
     */
    Impl(const SockAddr& sockAddr)
        : TcpSock::Impl(sockAddr)
    {
        if (sockAddr.getPort() == 0)
            throw LOGIC_ERROR("Port number of " + sockAddr.to_string() + " is "
                    "zero");

        sockAddr.connect(sd);
        rmtSockAddr = sockAddr;
    }
};

TcpClntSock::TcpClntSock(const SockAddr& sockAddr)
    : TcpSock(new Impl(sockAddr)) {
}

/******************************************************************************/

class UdpSock::Impl final : public Socket::Impl
{
    char          buf[MAX_PAYLOAD]; ///< UDP payload buffer
    size_t        bufLen;           ///< Number of read bytes in buffer
    struct pollfd pollfd;           ///< poll(2) structure

    /**
     * Reads the next UDP packet from the socket into the buffer.
     *
     * @retval    `false`       EOF or `halt()` called
     * @retval    `true`        Success
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
     * Constructs a sending UDP socket.
     *
     * @cancellationpoint
     */
    explicit Impl(const SockAddr& grpAddr)
        : Socket::Impl(grpAddr, SOCK_DGRAM, IPPROTO_UDP)
        , buf()
        , bufLen(0)
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
        : Impl(grpAddr)
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
    void setMcastIface(const InetAddr& iface) const {
        iface.setMcastIface(sd);
    }

    std::string to_string() const override {
        return "{sd=" + std::to_string(sd) + ", lcl=" + getLclAddr().to_string()
                + "proto=UDP, rmt=" + getRmtAddr().to_string() + "}";
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

UdpSock::UdpSock(const SockAddr& grpAddr)
    : Socket(new Impl(grpAddr))
{}

UdpSock::UdpSock(
        const SockAddr& grpAddr,
        const InetAddr& rmtAddr)
    : Socket(new Impl(grpAddr, rmtAddr))
{}

const UdpSock& UdpSock::setMcastIface(const InetAddr& iface) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->setMcastIface(iface);
    return *this;
}

} // namespace

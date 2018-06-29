/**
 * This file defines an SCTP socket. Reads on the socket are stateful; writes
 * are stateless.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "InetSockAddr.h"
#include "SctpSock.h"
#include "Thread.h"

#include <atomic>
#include <cstdint>
#include <errno.h>
#include <mutex>
/*
 * <netinet/in.h> must be included before <netinet/sctp.h> on Fedora 19 to avoid
 * a macro/enum name clash on IPPROTO_SCTP.
 */
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace hycast {

/**
 * Abstract SCTP socket.
 */
class BaseSctpSock::Impl
{
protected:
    const int sd;
    const int numStreams;

public:
    /**
     * Constructs.
     * @param[in] numStreams  Number of SCTP streams
     */
    Impl(const int numStreams)
        : sd(-1)
        , numStreams{numStreams}
    {}

    /**
     * Default constructs.
     * @throws SystemError if required memory can't be allocated
     */
    Impl()
        : Impl{0}
    {}

    /**
     * Constructs.
     * @param[in] sd         SCTP-compatible socket descriptor from `socket()`
     *                       or `accept()`
     * @param[in] numStream  Number of SCTP streams
     * @throws SystemError if required memory can't be allocated
     */
    Impl(   const int sd,
            const int numStreams)
        : sd{sd}
        , numStreams{numStreams}
    {
        if (sd < 0)
            throw INVALID_ARGUMENT("Invalid socket descriptor: " +
                    std::to_string(sd));
        if (numStreams < 0)
            throw INVALID_ARGUMENT("Invalid number of SCTP streams: " +
                    std::to_string(numStreams));
        struct sctp_event_subscribe events = {0};
        events.sctp_data_io_event = 1;
        if (::setsockopt(sd, IPPROTO_SCTP, SCTP_EVENTS, &events,
                sizeof(events)))
            throw SYSTEM_ERROR(
                    "setsockopt() failure: Couldn't subscribe to SCTP data I/O "
                    "events: sock=" + std::to_string(sd));
        struct sctp_initmsg sinit = {0};
        sinit.sinit_max_instreams = sinit.sinit_num_ostreams = numStreams;
        if (::setsockopt(sd, IPPROTO_SCTP, SCTP_INITMSG, &sinit,
                sizeof(sinit)))
            throw SYSTEM_ERROR(
                    "setsockopt() failure: Couldn't configure number of SCTP "
                    "streams: sock=" + std::to_string(sd) + ", numStreams=" +
                    std::to_string(numStreams));
    }

    /**
     * Prevents copy and move construction.
     */
    Impl(const Impl& socket) =delete;
    Impl(const Impl&& socket) =delete;

    /**
     * Prevents copy and move assignment.
     */
    Impl& operator=(const Impl& rhs) =delete;
    Impl& operator=(const Impl&& rhs) =delete;

    /**
     * Destroys an instance. Closes the underlying BSD socket if it's open.
     * @exceptionsafety Nothrow
     */
    virtual ~Impl() noexcept
    {
        try {
            if (sd >= 0 && ::close(sd))
                throw SYSTEM_ERROR("Couldn't close socket: sd=" +
                        std::to_string(sd));
        }
        catch (const std::exception& e) {
            log_error(e);
        }
    }

    /**
     * Returns the socket descriptor.
     * @return          Socket descriptor
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    int getSock() const noexcept
    {
    	return sd;
    }

    /**
     * Returns the number of SCTP streams.
     * @return           Number of SCTP streams
     * @exceptionsafety  Nothrow
     */
    unsigned getNumStreams() const noexcept
    {
        return numStreams;
    }

    /**
     * Returns the size of the send buffer.
     * @return  Size of the send buffer in bytes
     */
    int getSendBufSize() const
    {
        int       size;
        socklen_t len = sizeof(size);
        if (::getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, &len))
            throw SYSTEM_ERROR("::getsockopt() failure");
        return size;
    }

    /**
     * Sets the size of the send buffer.
     * @param[in] size     Send buffer size in bytes
     * @return             Reference to this instance
     * @throw SystemError  Size couldn't be set
     */
    Impl& setSendBufSize(const int size)
    {
        if (::setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)))
            throw SYSTEM_ERROR("::setsockopt() failure");
        return *this;
    }

    /**
     * Returns the size of the receive buffer.
     * @return  Size of the receive buffer in bytes
     */
    int getRecvBufSize() const
    {
        int       size;
        socklen_t len = sizeof(size);
        if (::getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, &len))
            throw SYSTEM_ERROR("::getsockopt() failure");
        return size;
    }

    /**
     * Sets the size of the receive buffer.
     * @param[in] size     Receive buffer size in bytes
     * @return             Reference to this instance
     * @throw SystemError  Size couldn't be set
     */
    Impl& setRecvBufSize(const int size)
    {
        if (::setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)))
            throw SYSTEM_ERROR("::setsockopt() failure");
        return *this;
    }

    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @retval `true`   This instance equals the other
     * @retval `false`  This instance doesn't equal the other
     * @exceptionsafety Nothrow
     */
    bool operator==(const Impl& that) const noexcept
    {
        return (sd == that.sd) && (numStreams == that.numStreams);
    }

    /**
     * Returns a string representation of this instance.
     * @return String representation of this instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const
    {
        return "{sd=" + std::to_string(sd) + ", numStreams=" +
                std::to_string(numStreams) + "}";
    }
}; // `BaseSctpSock::Impl`

int BaseSctpSock::createSocket()
{
    auto sd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (sd == -1)
          throw SYSTEM_ERROR("Couldn't create SCTP socket");
    return sd;
}

BaseSctpSock::BaseSctpSock(Impl* impl)
    : pImpl{impl}
{}

BaseSctpSock::~BaseSctpSock() noexcept
{}

int BaseSctpSock::getSock() const noexcept
{
    return pImpl->getSock();
}

uint16_t BaseSctpSock::getNumStreams() const
{
    return pImpl->getNumStreams();
}

int BaseSctpSock::getSendBufSize() const
{
    return pImpl->getSendBufSize();
}

BaseSctpSock& BaseSctpSock::setSendBufSize(const int size)
{
    pImpl->setSendBufSize(size);
    return *this;
}

int BaseSctpSock::getRecvBufSize() const
{
    return pImpl->getRecvBufSize();
}

BaseSctpSock& BaseSctpSock::setRecvBufSize(const int size)
{
    pImpl->setRecvBufSize(size);
    return *this;
}

bool BaseSctpSock::operator ==(const BaseSctpSock& that) const noexcept
{
    return pImpl->operator==(*that.pImpl.get());
}

std::string BaseSctpSock::to_string() const
{
    return pImpl->to_string();
}

/******************************************************************************/

/**
 * Server-side SCTP socket implementation.
 */
class SrvrSctpSock::Impl : public BaseSctpSock::Impl
{
public:
    Impl()
        : BaseSctpSock::Impl{}
    {}

    /**
     * Constructs. The socket is bound to the Internet socket address but
     * `listen()` is not called to allow certain options to be set (e.g., buffer
     * sizes).
     * @param[in] addr        Internet socket address on which to listen
     * @param[in] numStreams  Number of SCTP streams
     * @throw RuntimeError    Couldn't construct server-size SCTP socket
     * @see   `listen()`
     */
    Impl(   const InetSockAddr& addr,
            const int           numStreams)
        : BaseSctpSock::Impl{createSocket(), numStreams}
    {
        try {
            const int enable = 1;
            if (::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable,
                    sizeof(enable)))
                throw SYSTEM_ERROR(
                        "setsockopt(SO_REUSEADDR) failure: sd=" +
                        std::to_string(sd) + ", addr=" + addr.to_string());
            addr.bind(sd);
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't construct server-side SCTP socket"));
        }
    }

    /**
     * Configures the socket for accepting incoming association attempts.
     * @param[in] queueSize  Limit on the number of outstanding attempts
     *                       in the `::listen()` queue
     * @throw LogicError     Socket already configured for listening
     * @throw SystemError    `::listen()` failure
     */
    void listen(const int queueSize = 5)
    {
        if (::listen(sd, queueSize))
            throw SYSTEM_ERROR(
                    "listen() failure: sd=" + std::to_string(sd) +
                    ", queueSize=" + std::to_string(queueSize));
    }

    /**
     * Accepts an incoming connection on the socket.
     * @return             The accepted connection
     * @throw SystemError  `accept()` failure
     * @throw SystemError  Required memory can't be allocated
     * @exceptionsafety    Basic guarantee
     * @threadsafety       Thread-compatible but not thread-safe
     */
    SctpSock accept()
    {
        int             newSd;
        struct sockaddr addr;
        {
            socklen_t       len = sizeof(addr);
            Canceler        canceler{};
            newSd = ::accept(sd, &addr, &len);
        }
        if (newSd < 0)
            throw SYSTEM_ERROR("accept() failure: sd=" + std::to_string(sd));
        try {
            return SctpSock{newSd, addr, numStreams};
        }
        catch (const std::exception& e) {
            (void)::close(newSd);
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't create an established SCTP socket"));
        }
        return SctpSock{}; // NOTREACHED. Pacifies Eclipse
    }
};

SrvrSctpSock::SrvrSctpSock()
    : BaseSctpSock{new Impl()}
{}

SrvrSctpSock::SrvrSctpSock(
        const InetSockAddr& addr,
        const int           numStreams)
    : BaseSctpSock{new Impl(addr, numStreams)}
{}

SrvrSctpSock& SrvrSctpSock::operator =(const SrvrSctpSock& rhs)
{
    if (pImpl.get() != rhs.pImpl.get())
        pImpl = rhs.pImpl;
    return *this;
}

void SrvrSctpSock::listen(const int queueSize) const
{
    (static_cast<Impl*>(pImpl.get()))->listen(queueSize);
}

SctpSock SrvrSctpSock::accept() const
{
    return (static_cast<Impl*>(pImpl.get()))->accept();
}

/******************************************************************************/

/**
 * Established SCTP socket.
 */
class SctpSock::Impl : public BaseSctpSock::Impl
{
private:
    /**
     * RAII class for ignoring SIGPIPE.
     */
    class IgnoreSigPipe
    {
        struct sigaction oldSigact;
    public:
        IgnoreSigPipe()
        {
            struct sigaction ignoreSignal = {0};
            ignoreSignal.sa_handler = SIG_IGN;
            if (::sigaction(SIGPIPE, &ignoreSignal, &oldSigact))
                throw SYSTEM_ERROR("Couldn't ignore SIGPIPE");
        }
        IgnoreSigPipe(const IgnoreSigPipe& that) =delete;
        IgnoreSigPipe& operator=(const IgnoreSigPipe& rhs) =delete;
        ~IgnoreSigPipe() noexcept
        {
            if (::sigaction(SIGPIPE, &oldSigact, nullptr))
                log_error(SYSTEM_ERROR("Couldn't restore SIGPIPE handling"));
        }
    };
    typedef std::mutex             Mutex;
    typedef std::lock_guard<Mutex> LockGuard;

    Mutex                          mutex;       // For input state
    unsigned                       streamId;    // Part of input state
    uint32_t                       size;        // Part of input state
    bool                           haveCurrMsg; // Part of input state
    InetSockAddr                   remoteAddr;

    /**
     * Throws an exception if the socket isn't ready for writing.
     * @throw RuntimeError  Socket isn't ready for writing
     * @throw SystemError   `poll()` failure
     */
    void throwIfNotWritable()
    {
#if 0
        struct pollfd pollfd;
        pollfd.fd = sd;
        pollfd.events = POLLOUT;
        const int status = ::poll(&pollfd, 1, 0); // 0 => immediate return
        if (status == -1)
            throw SYSTEM_ERROR("poll() failure");
        if (status == 0)
            throw RUNTIME_ERROR("Socket not ready for writing");
#endif
    }

    /**
     * Checks the return status of an I/O function.
     * @param[in] line      Line number of I/O function
     * @param[in] funcName  Name of I/O function
     * @param[in] expected  The expected return status or 0, which disables the
     *                      comparison with `actual`
     * @param[in] actual    The actual return status
     * @throws SystemError  if `actual < 0 || (expected && actual != expected)`
     * @exceptionsafety Strong
     */
    void checkIoStatus(
            const int     line,
            const char*   funcName,
            const size_t  expected,
            const ssize_t actual) const
    {
        if (actual < 0)
            throw SystemError(__FILE__, line, std::string(funcName) +
                    " failure: sd=" + std::to_string(sd) + ", expected=" +
                    std::to_string(expected) + ", errno=" +
                    std::to_string(errno));
    }

    /**
     * Gets information on the next SCTP message. The message is left in the
     * socket's input buffer.
     * @throws SystemError if an I/O error occurs
     * @exceptionsafety    Basic
     * @threadsafety       Safe
     */
    void getNextMsgInfo()
    {
        int                    numRecvd;
        struct sctp_sndrcvinfo sinfo;
        {
            int       flags = MSG_PEEK;
            char      msg[1];
            socklen_t socklen = 0;
            /*
             * According to `man sctp_recvmsg` and
             * <https://tools.ietf.org/html/rfc6458>, `sctp_recvmsg()` returns
             * the number of bytes "received". Empirically, this is *not*
             * greater than the number of bytes requested (i.e., it is *not* the
             * number of bytes in the message -- even if MSG_PEEK is specified).
             * Apparently, it's the total number of bytes written to the buffers.
             */
            Canceler canceler{};
            numRecvd = sctp_recvmsg(sd, msg, sizeof(msg), nullptr, &socklen,
                    &sinfo, &flags);
        }
        if (numRecvd == 0 ||
                (numRecvd == -1 && (errno == ECONNRESET || errno == ENOTCONN))) {
            size = 0; // EOF
        }
        else {
            checkIoStatus(__LINE__, "getNextMsgInfo()->sctp_recvmsg()", 0,
                    numRecvd);
            streamId = sinfo.sinfo_stream;
            size = ntohl(sinfo.sinfo_ppid);
        }
        haveCurrMsg = true;
    }

    /**
     * Returns the size, in bytes, of the current SCTP message while the mutex
     * is locked. Waits for the next message if necessary.
     * @pre    `mutex` is locked
     * @return Size of current message in bytes or 0 if the remote peer closed
     *         the socket.
     */
    uint32_t lockedGetSize()
    {
        ensureMsg();
        return size;
    }

    /**
     * Computes the total number of bytes in a scatter/gather IO operation.
     * @param[in] iovec   Scatter/gather IO vector
     * @param[in] iovcnt  Number of elements in `iovec`
     */
    static size_t iovLen(
        const struct iovec* iovec,
        const int           iovcnt) noexcept
    {
        size_t len = 0;
        for (int i = 0; i < iovcnt; i++)
            len += iovec[i].iov_len;
        return len;
    }

    /**
     * Initializes an SCTP send/receive information structure.
     * @param[out] sinfo     SCTP send/receive information structure
     * @param[in]  streamId  SCTP stream number
     * @param[in]  size      Size of the message in bytes
     * @exceptionsafety      Nothrow
     */
    static void sndrcvinfoInit(
            struct sctp_sndrcvinfo& sinfo,
            const unsigned          streamId,
            const size_t            size) noexcept
    {
        ::memset(&sinfo, 0, sizeof(sinfo));
        sinfo.sinfo_stream = streamId;
        sinfo.sinfo_flags = SCTP_UNORDERED;
        sinfo.sinfo_ppid = htonl(size);
        sinfo.sinfo_timetolive = 30000; // in ms
    }

    /**
     * Receives a message while the mutex is locked.
     * @pre                `mutex` is locked
     * @param[out] msg     Receive buffer
     * @param[in]  len     Size of receive buffer in bytes
     * @param[in]  flags   Type of message reception. Logical OR of zero or
     *                     more of
     *                       - `MSG_OOB`  Requests out-of-band data
     *                       - `MSG_PEEK` Peeks at the incoming message
     * @throws SystemError I/O failure or number of bytes read doesn't equal
     *                     `len`
     * @exceptionsafety    Basic
     * @threadsafety       Safe
     */
    void lockedRecv(
            void*        msg,
            const size_t len,
            const int    flags = 0)
    {
        /*
         * NB: If the current message exists and `len` is less than the size of the
         * message, then the message will continue to be the current message --
         * regardless of whether or not MSG_PEEK is specified. See `discard()`.
         */
        struct sctp_sndrcvinfo  sinfo;
        int                     numRead;
        socklen_t               socklen = 0;
        {
            int      tmpFlags = flags;
            Canceler canceler{};
            numRead = sctp_recvmsg(sd, msg, len, nullptr, &socklen, &sinfo,
                    &tmpFlags);
        }
        checkIoStatus(__LINE__, "sctp_recvmsg()", len, numRead);
        haveCurrMsg = (flags & MSG_PEEK) != 0;
    }

    /**
     * Ensures that the current message exists.
     * @pre                `mutex` is locked
     * @throws SystemError if an I/O error occurs
     * @exceptionsafety    Basic
     * @threadsafety       Safe
     */
    void ensureMsg()
    {
        if (!haveCurrMsg)
            getNextMsgInfo();
    }

public:
    /**
     * Default constructs.
     */
    Impl()
        : BaseSctpSock::Impl{0}
        , mutex{}
        , streamId(0)
        , size(0)
        , haveCurrMsg(false)
        , remoteAddr{}
    {}

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
    Impl(const int numStreams)
        : BaseSctpSock::Impl{createSocket(), numStreams}
        , mutex{}
        , streamId(0)
        , size(0)
        , haveCurrMsg(false)
        , remoteAddr{}
    {}

    /**
     * Constructs an SCTP socket from the server side.
     * @param[in] sd         SCTP socket descriptor from `accept()`
     * @param[in] addr       Address of the remote SCTP socket
     * @param[in] numStream  Number of SCTP streams
     * @throws SystemError   Required memory can't be allocated
     */
    Impl(   const int              sd,
            const struct sockaddr& addr,
            const int              numStreams)
        : BaseSctpSock::Impl(sd, numStreams)
        , mutex{}
        , streamId(0)
        , size(0)
        , haveCurrMsg(false)
        , remoteAddr{InetSockAddr{addr}}
    {}

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
    Impl& connect(const InetSockAddr& addr)
    {
        addr.connect(sd);
        remoteAddr = addr;
        return *this;
    }

    /**
     * Returns the Internet socket address of the remote end.
     * @return Internet socket address of the remote end
     */
    inline InetSockAddr getRemoteAddr() const noexcept
    {
        return remoteAddr;
    }

    /**
     * Sends a message.
     * @param[in] streamId   SCTP stream number
     * @param[in] msg        Message to be sent
     * @param[in] len        Size of message in bytes
     * @throws RuntimeError  The socket isn't ready for writing
     * @throws SystemError   An I/O error occurred
     * @exceptionsafety      Strong guarantee
     * @threadsafety         Safe
     */
    void send(
            const unsigned streamId,
            const void*    msg,
            const size_t   len)
    {
        struct sctp_sndrcvinfo sinfo;
        sndrcvinfoInit(sinfo, streamId, len);
        int sendStatus;
        {
            Canceler canceler{};
            throwIfNotWritable();
            sendStatus = sctp_send(sd, msg, len, &sinfo, MSG_EOR);
        }
        checkIoStatus(__LINE__, "sctp_send()", len, sendStatus);
    }

    /**
     * Sends a message. This is a cancellation point.
     * @param[in] streamId   SCTP stream number
     * @param[in] iovec      Vector comprising message to send
     * @param[in] iovcnt     Number of elements in `iovec`
     * @throws RuntimeError  The socket isn't ready for writing
     * @throws SystemError   An I/O error occurred
     * @exceptionsafety      Strong guarantee
     * @threadsafety         Safe
     */
    void sendv(
            const unsigned streamId,
            struct iovec*  iovec,
            const int      iovcnt)
    {
        ssize_t numExpected = iovLen(iovec, iovcnt);
        struct {
            struct cmsghdr         cmsghdr;
            struct sctp_sndrcvinfo sinfo;
        } msg_control = {0};
        msg_control.cmsghdr.cmsg_len = sizeof(msg_control);
        msg_control.cmsghdr.cmsg_level = IPPROTO_SCTP;
        msg_control.cmsghdr.cmsg_type = SCTP_SNDRCV;
        sndrcvinfoInit(msg_control.sinfo, streamId, numExpected);
        struct msghdr msghdr = {0};
        msghdr.msg_iov = iovec;
        msghdr.msg_iovlen = iovcnt;
        msghdr.msg_control = &msg_control;
        msghdr.msg_controllen = sizeof(msg_control);
        ssize_t sendStatus;
        {
            // IgnoreSigPipe ignoreSigPipe{}; // Replaced by MSG_NOSIGNAL
            Canceler      canceler{};
            throwIfNotWritable();
            sendStatus = ::sendmsg(sd, &msghdr, MSG_EOR | MSG_NOSIGNAL);
        }
        checkIoStatus(__LINE__, "sendmsg()", numExpected, sendStatus);
    }

    /**
     * Returns the size, in bytes, of the current SCTP message. Waits for the
     * next message if necessary.
     * @return Size of current message in bytes or 0 if the remote peer closed
     *         the socket.
     */
    uint32_t getSize()
    {
        LockGuard lock{mutex};
        return lockedGetSize();
    }

    /**
     * Returns the SCTP stream number of the current SCTP message. Waits for the
     * next message if necessary.
     * @return SCTP stream number of current message.
     * @throws std::system_error if an I/O error occurs
     */
    unsigned getStreamId()
    {
        LockGuard lock{mutex};
        ensureMsg();
        return streamId;
    }

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
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    void recv(
            void*        msg,
            const size_t len,
            const int    flags = 0)
    {
        LockGuard lock{mutex};
        lockedRecv(msg, len, flags);
    }

    /**
     * Receives a message.
     * @param[in] iovec     Vector comprising message receive buffers
     * @param[in] iovcnt    Number of elements in `iovec`
     * @param[in] flags     Type of message reception. Logical OR of zero or
     *                      more of
     *                      - `MSG_OOB`  Requests out-of-band data
     *                      - `MSG_PEEK` Peeks at the incoming message
     * @return              Number of bytes actually read
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic guarantee
     * @threadsafety Safe
     */
    size_t recvv(
            const struct iovec* iovec,
            const int           iovcnt,
            const int           flags = 0)
    {
        LockGuard lock{mutex};
        ssize_t numExpected = iovLen(iovec, iovcnt);
        struct msghdr msghdr = {};
        msghdr.msg_iov = const_cast<struct iovec*>(iovec);
        msghdr.msg_iovlen = iovcnt;
        ssize_t numRead;
        {
            Canceler canceler{};
            numRead = ::recvmsg(sd, &msghdr, flags);
        }
        checkIoStatus(__LINE__, "recvmsg()", numExpected, numRead);
        haveCurrMsg = (flags & MSG_PEEK) != 0;
        return numRead;
    }

    /**
     * Indicates if this instance has a current message.
     * @retval true   Yes
     * @retval false  No
     */
    bool hasMessage()
    {
        LockGuard lock{mutex};
        return haveCurrMsg;
    }

    /**
     * Discards the current message.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void discard()
    {
        LockGuard lock{mutex};
        if (haveCurrMsg) {
            /*
             * A message on an SCTP socket must be read in its entirety in order for
             * it to be discarded. This is in contrast to a message on a
             * "message-based" socket, such as SOCK_DGRAM, in which excess bytes
             * beyond the requested are discarded. Recall that the SCTP socket-type
             * is SOCK_STREAM. See `recv()`.
             */
            char msg[lockedGetSize()];
            lockedRecv(msg, sizeof(msg));
        }
    }
};


SctpSock::SctpSock()
    : BaseSctpSock{new Impl()}
{}

SctpSock::SctpSock(
        const int              sd,
        const struct sockaddr& addr,
        const int              numStreams)
    : BaseSctpSock{new Impl(sd, addr, numStreams)}
{}

SctpSock::SctpSock(const int numStreams)
    : BaseSctpSock{new Impl(numStreams)}
{}

SctpSock& SctpSock::operator =(const SctpSock& rhs)
{
    if (pImpl.get() != rhs.pImpl.get())
        pImpl = rhs.pImpl;
    return *this;
}

void SctpSock::discard() const
{
    (static_cast<Impl*>(pImpl.get()))->discard();
}

SctpSock& SctpSock::connect(const InetSockAddr& addr)
{
    (static_cast<Impl*>(pImpl.get()))->connect(addr);
    return *this;
}

InetSockAddr SctpSock::getRemoteAddr() const
{
    return (static_cast<Impl*>(pImpl.get()))->getRemoteAddr();
}

unsigned SctpSock::getStreamId() const
{
    return (static_cast<Impl*>(pImpl.get()))->getStreamId();
}

uint32_t SctpSock::getSize() const
{
    return (static_cast<Impl*>(pImpl.get()))->getSize();
}

void SctpSock::send(
        const unsigned streamId,
        const void*    msg,
        const size_t   len) const
{
    (static_cast<Impl*>(pImpl.get()))->send(streamId, msg, len);
}

void SctpSock::sendv(
        const unsigned streamId,
        struct iovec*  iovec,
        const int      iovcnt) const
{
    (static_cast<Impl*>(pImpl.get()))->sendv(streamId, iovec, iovcnt);
}

void SctpSock::recv(
        void*        msg,
        const size_t len,
        const int    flags) const
{
    (static_cast<Impl*>(pImpl.get()))->recv(msg, len, flags);
}

size_t SctpSock::recvv(
        const struct iovec* iovec,
        const int           iovcnt,
        const int           flags) const
{
    return (static_cast<Impl*>(pImpl.get()))->recvv(iovec, iovcnt, flags);
}

bool SctpSock::hasMessage() const
{
    return (static_cast<Impl*>(pImpl.get()))->hasMessage();
}

} // namespace

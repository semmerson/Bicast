/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an SCTP socket.
 */
#include "config.h"

#include "error.h"
#include "InetSockAddr.h"
#include "SctpSock.h"

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
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace hycast {

class SctpSock::Impl
{
protected:
    std::atomic_int sock;

private:
    unsigned                streamId;
    uint32_t                size;
    bool                    haveCurrMsg;
    unsigned                numStreams;
    std::mutex              readMutex;
    std::mutex              writeMutex;
    InetSockAddr            remoteAddr;
    static struct sigaction sigact;

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
        int sd = sock.load();
        if (actual < 0)
            throw SystemError(__FILE__, line, std::string(funcName) +
                    " failure: sock=" + std::to_string(sd) + ", expected=" +
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
            std::lock_guard<std::mutex> lock(readMutex);
            int sd = sock.load();
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
        sinfo.sinfo_stream = streamId;
        sinfo.sinfo_flags = SCTP_UNORDERED;
        sinfo.sinfo_ppid = htonl(size);
        sinfo.sinfo_timetolive = 30000; // in ms
    }

    /**
     * Ensures that the current message exists.
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
     * Constructs from nothing.
     * @throws SystemError if required memory can't be allocated
     */
    Impl()
        : sock(-1)
        , streamId(0)
        , size(0)
        , haveCurrMsg(false)
        , numStreams(0)
        , readMutex()
        , writeMutex()
        , remoteAddr{}
    {
        sigact.sa_handler = SIG_IGN;
    }

    /**
     * Constructs from a socket and the number of SCTP streams. If the socket
     * isn't connected to a remote endpoint, then getRemoteAddr() will return
     * a default-constructed `InetSockAddr`.
     * @param[in] sd            Socket descriptor
     * @param[in] numStreams    Number of SCTP streams
     * @throws InvalidArgument  `sock < 0 || numStreams > UINT16_MAX`
     * @throws SystemError      Socket couldn't be configured
     * @see getRemoteAddr()
     */
    Impl(   const int      sd,
            const unsigned numStreams)
        : Impl()
    {
        if (sd < 0)
            throw InvalidArgument(__FILE__, __LINE__,
                    "Invalid socket: " + std::to_string(sd));
        if (numStreams > UINT16_MAX)
            throw InvalidArgument(__FILE__, __LINE__,
                    "Invalid number of streams: " + std::to_string(numStreams));
        sock = sd;
        this->numStreams = numStreams;
        struct sctp_event_subscribe events = {0};
        events.sctp_data_io_event = 1;
        int status = ::setsockopt(sd, IPPROTO_SCTP, SCTP_EVENTS, &events,
                sizeof(events));
        if (status)
            throw SystemError(__FILE__, __LINE__,
                    "setsockopt() failure: Couldn't subscribe to SCTP data I/O "
                    "events: sock=" + std::to_string(sd));
        struct sctp_initmsg sinit = {0};
        sinit.sinit_max_instreams = sinit.sinit_num_ostreams = numStreams;
        status = ::setsockopt(sd, IPPROTO_SCTP, SCTP_INITMSG, &sinit,
                sizeof(sinit));
        if (status)
            throw SystemError(__FILE__, __LINE__,
                    "setsockopt() failure: Couldn't configure number of SCTP "
                    "streams: sock=" + std::to_string(sd) + ", numStreams=" +
                    std::to_string(numStreams));
        struct sockaddr addr;
        socklen_t       len = sizeof(addr);
        status = ::getpeername(sd, &addr, &len);
        remoteAddr = status
                        ? std::move(InetSockAddr())
                        : std::move(InetSockAddr(addr));
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
     * Destroys an instance. Closes the underlying BSD socket it it's open.
     * @exceptionsafety Nothrow
     */
    ~Impl() noexcept
    {
        int sd = sock.load();
        if (sd >= 0) {
            auto status = ::close(sd);
            if (status)
                log_what(SystemError(__FILE__, __LINE__,
                        "Couldn't close socket: sd=" + std::to_string(sd),
                        status));
            sock = -1;
        }
    }

    /**
     * Returns the socket descriptor.
     * @return Socket descriptor
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    int getSock() const noexcept
    {
    	return sock;
    }

    /**
     * Returns the number of SCTP streams.
     * @return the number of SCTP streams
     */
    unsigned getNumStreams()
	{
		return numStreams;
	}

    /**
     * Returns the Internet socket address of the remote end.
     * @return Internet socket address of the remote end
     */
    const InetSockAddr& getRemoteAddr()
	{
		return remoteAddr;
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
		return sock.load() == that.sock.load();
	}

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const
	{
		return std::string("SocketImpl{sock=") + std::to_string(sock.load()) + "}";
	}

    /**
     * Sends a message.
     * @param[in] streamId  SCTP stream number
     * @param[in] msg       Message to be sent
     * @param[in] len       Size of message in bytes
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
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
			std::lock_guard<std::mutex> lock(writeMutex);
			int sd = sock.load();
			sendStatus = sctp_send(sd, msg, len, &sinfo, MSG_EOR);
		}
		checkIoStatus(__LINE__, "sctp_send()", len, sendStatus);
	}

    /**
     * Sends a message.
     * @param[in] streamId  SCTP stream number
     * @param[in] iovec     Vector comprising message to send
     * @param[in] iovcnt    Number of elements in `iovec`
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    void sendv(
            const unsigned      streamId,
            const struct iovec* iovec,
            const int           iovcnt)
	{
		ssize_t numExpected = iovLen(iovec, iovcnt);
		struct {
			struct cmsghdr         cmsghdr;
			struct sctp_sndrcvinfo sinfo;
		} msg_control;
		msg_control.cmsghdr.cmsg_len = sizeof(msg_control);
		msg_control.cmsghdr.cmsg_level = IPPROTO_SCTP;
		msg_control.cmsghdr.cmsg_type = SCTP_SNDRCV;
		sndrcvinfoInit(msg_control.sinfo, streamId, numExpected);
		struct msghdr msghdr = {0};
		msghdr.msg_iov = const_cast<struct iovec*>(iovec);
		msghdr.msg_iovlen = iovcnt;
		msghdr.msg_control = &msg_control;
		msghdr.msg_controllen = sizeof(msg_control);
		ssize_t sendStatus;
		{
			std::lock_guard<std::mutex> lock(writeMutex);
			int sd = sock.load();
			struct sigaction oact;
			::sigaction(SIGPIPE, &sigact, &oact);
			sendStatus = ::sendmsg(sd, &msghdr, MSG_EOR);
			::sigaction(SIGPIPE, &oact, nullptr);
		}
		checkIoStatus(__LINE__, "sendmsg()", numExpected, sendStatus);
	}

    /**
     * Returns the size, in bytes, of the current SCTP message. Waits for the
     * next message if necessary.
     * @return Size of current message in bytes or 0 if the remove peer closed
     *         the socket.
     */
    uint32_t getSize()
	{
		ensureMsg();
		return size;
	}

    /**
     * Returns the SCTP stream number of the current SCTP message. Waits for the
     * next message if necessary.
     * @return SCTP stream number of current message.
     * @throws std::system_error if an I/O error occurs
     */
    unsigned getStreamId()
	{
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
		/*
		 * NB: If the current message exists and `len` is less than the size of the
		 * message, then the message will continue to be the current message --
		 * regardless of whether or not MSG_PEEK is specified. See `discard()`.
		 */
		struct sctp_sndrcvinfo  sinfo;
		int                     numRead;
		socklen_t               socklen = 0;
		{
			int tmpFlags = flags;
			std::lock_guard<std::mutex> lock(readMutex);
			int sd = sock.load();
			numRead = sctp_recvmsg(sd, msg, len, nullptr, &socklen, &sinfo,
					&tmpFlags);
		}
		checkIoStatus(__LINE__, "sctp_recvmsg()", len, numRead);
		haveCurrMsg = (flags & MSG_PEEK) != 0;
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
		ssize_t numExpected = iovLen(iovec, iovcnt);
		struct msghdr msghdr = {};
		msghdr.msg_iov = const_cast<struct iovec*>(iovec);
		msghdr.msg_iovlen = iovcnt;
		ssize_t numRead;
		{
			std::lock_guard<std::mutex> lock(readMutex);
			int sd = sock.load();
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
		return haveCurrMsg;
	}

    /**
     * Discards the current message.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void discard()
	{
		if (haveCurrMsg) {
			/*
			 * A message on an SCTP socket must be read in its entirety in order for
			 * it to be discarded. This is in contrast to a message on a
			 * "message-based" socket, such as SOCK_DGRAM, in which excess bytes
			 * beyond the requested are discarded. Recall that the SCTP socket-type
			 * is SOCK_STREAM. See `recv()`.
			 */
			char msg[getSize()];
			recv(msg, sizeof(msg));
		}
	}

    /**
     * Closes the underlying BSD socket. Idempotent.
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    void close() noexcept
    {
        int sd = sock.load();
        if (sd >= 0) {
            (void)::close(sd);
            sock = -1;
        }
    }
};

struct sigaction SctpSock::Impl::sigact;

int SctpSock::createSocket()
{
      auto sd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
      if (sd == -1)
          throw SystemError(__FILE__, __LINE__, "Couldn't create SCTP socket",
                  sd);
      return sd;
}

SctpSock::SctpSock()
    : pImpl(new Impl())
{}

SctpSock::SctpSock(
        const int      sd,
        const uint16_t numStreams)
    : pImpl(new Impl(sd, numStreams))
{}

SctpSock::SctpSock(Impl* impl)
    : pImpl(impl)
{}

SctpSock::SctpSock(std::shared_ptr<Impl> sptr)
    : pImpl(sptr)
{}

int SctpSock::getSock() const noexcept
{
	return pImpl->getSock();
}

uint16_t SctpSock::getNumStreams() const
{
    return pImpl->getNumStreams();
}

const InetSockAddr& SctpSock::getRemoteAddr()
{
    return pImpl->getRemoteAddr();
}

bool SctpSock::operator ==(const SctpSock& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

unsigned SctpSock::getStreamId() const
{
    return pImpl->getStreamId();
}

uint32_t SctpSock::getSize() const
{
    return pImpl->getSize();
}

std::string SctpSock::to_string() const
{
    return pImpl->to_string();
}

void SctpSock::send(
        const unsigned streamId,
        const void*    msg,
        const size_t   len) const
{
    pImpl->send(streamId, msg, len);
}

void SctpSock::sendv(
        const unsigned      streamId,
        const struct iovec* iovec,
        const int           iovcnt) const
{
    pImpl->sendv(streamId, iovec, iovcnt);
}

void SctpSock::recv(
        void*        msg,
        const size_t len,
        const int    flags) const
{
    pImpl->recv(msg, len, flags);
}

size_t SctpSock::recvv(
        const struct iovec* iovec,
        const int           iovcnt,
        const int           flags) const
{
    return pImpl->recvv(iovec, iovcnt, flags);
}

bool SctpSock::hasMessage() const
{
    return pImpl->hasMessage();
}

void SctpSock::discard() const
{
    pImpl->discard();
}

void SctpSock::close() const
{
    pImpl->close();
}

} // namespace

/**
 * This file implements an SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: SctpSockImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "SctpSockImpl.h"

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
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace hycast {

void SctpSockImpl::checkIoStatus(
        const char*   funcName,
        const size_t  expected,
        const ssize_t actual) const
{
    int sd = sock.load();
    if (actual < 0)
        throw std::system_error(errno, std::system_category(),
                std::string(funcName) + " failure: sock=" + std::to_string(sd)
                + ", expected=" + std::to_string(expected) + ", errno=" +
                std::to_string(errno));
}

void SctpSockImpl::getNextMsgInfo()
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
        checkIoStatus("getNextMsgInfo()->sctp_recvmsg()", 0, numRecvd);
        streamId = sinfo.sinfo_stream;
        size = ntohl(sinfo.sinfo_ppid);
    }
    haveCurrMsg = true;
}

size_t SctpSockImpl::iovLen(
        const struct iovec* iovec,
        const int           iovcnt) noexcept
{
    size_t len = 0;
    for (int i = 0; i < iovcnt; i++)
        len += iovec[i].iov_len;
    return len;
}

void SctpSockImpl::sndrcvinfoInit(
        struct sctp_sndrcvinfo& sinfo,
        const unsigned          streamId,
        const size_t            size) noexcept
{
    sinfo.sinfo_stream = streamId;
    sinfo.sinfo_flags = SCTP_UNORDERED;
    sinfo.sinfo_ppid = htonl(size);
    sinfo.sinfo_timetolive = 30000; // in ms
}

void SctpSockImpl::ensureMsg()
{
    if (!haveCurrMsg)
        getNextMsgInfo();
}

SctpSockImpl::SctpSockImpl()
    : sock(-1)
    , streamId(0)
    , size(0)
    , haveCurrMsg(false)
    , numStreams(0)
    , readMutex()
    , writeMutex()
    , remoteAddr{}
{}

SctpSockImpl::SctpSockImpl(
        const int      sd,
        const unsigned numStreams)
    : sock(sd)
    , streamId(0)
    , size(0)
    , haveCurrMsg(false)
    , numStreams(numStreams)
    , readMutex()
    , writeMutex()
    , remoteAddr{}
{
    if (sd < 0)
        throw std::invalid_argument("Invalid socket: " + std::to_string(sd));
    if (numStreams > UINT16_MAX)
        throw std::invalid_argument("Invalid number of streams: " +
                std::to_string(numStreams));
    struct sctp_event_subscribe events = {0};
    events.sctp_data_io_event = 1;
    int status = ::setsockopt(sd, IPPROTO_SCTP, SCTP_EVENTS, &events,
            sizeof(events));
    if (status)
        throw std::system_error(errno, std::system_category(),
                "setsockopt() failure: Couldn't subscribe to SCTP data I/O "
                "events: sock=" + std::to_string(sd));
    struct sctp_initmsg sinit = {0};
    sinit.sinit_max_instreams = sinit.sinit_num_ostreams = numStreams;
    status = ::setsockopt(sd, IPPROTO_SCTP, SCTP_INITMSG, &sinit,
            sizeof(sinit));
    if (status)
        throw std::system_error(errno, std::system_category(),
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

SctpSockImpl::~SctpSockImpl()
{
    this->close();
}

unsigned SctpSockImpl::getNumStreams() {
    return numStreams;
}

const InetSockAddr& SctpSockImpl::getRemoteAddr()
{
    return remoteAddr;
}

bool SctpSockImpl::operator==(const SctpSockImpl& that) const noexcept {
    return sock.load() == that.sock.load();
}

std::string SctpSockImpl::to_string() const {
    return std::string("SocketImpl{sock=") + std::to_string(sock.load()) + "}";
}

void SctpSockImpl::send(
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
    checkIoStatus("sctp_send()", len, sendStatus);
}

void SctpSockImpl::sendv(
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
        sendStatus = sendmsg(sd, &msghdr, MSG_EOR);
    }
    checkIoStatus("sendmsg()", numExpected, sendStatus);
}

uint32_t SctpSockImpl::getSize()
{
    ensureMsg();
    return size;
}

unsigned SctpSockImpl::getStreamId()
{
    ensureMsg();
    return streamId;
}

void SctpSockImpl::recv(
        void*        msg,
        const size_t len,
        const int    flags)
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
        numRead = sctp_recvmsg(sd, msg, len, (struct sockaddr*)nullptr,
                &socklen, &sinfo, &tmpFlags);
    }
    checkIoStatus("sctp_recvmsg()", len, numRead);
    haveCurrMsg = (flags & MSG_PEEK) != 0;
}

size_t SctpSockImpl::recvv(
        const struct iovec* iovec,
        const int           iovcnt,
        const int           flags)
{
    ssize_t numExpected = iovLen(iovec, iovcnt);
    struct msghdr msghdr = {};
    msghdr.msg_iov = const_cast<struct iovec*>(iovec);
    msghdr.msg_iovlen = iovcnt;
    ssize_t numRead;
    {
        std::lock_guard<std::mutex> lock(readMutex);
        int sd = sock.load();
        numRead = recvmsg(sd, &msghdr, flags);
    }
    checkIoStatus("recvmsg()", numExpected, numRead);
    haveCurrMsg = (flags & MSG_PEEK) != 0;
    return numRead;
}

bool SctpSockImpl::hasMessage()
{
    return haveCurrMsg;
}

void SctpSockImpl::discard()
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

void SctpSockImpl::close()
{
    int sd = sock.load();
    if (sd >= 0) {
        (void)::close(sd);
        sock = -1;
    }
}

} // namespace




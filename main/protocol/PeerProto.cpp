/**
 * Peer-to-peer connection protocol.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerProto.cpp
 *  Created on: Nov 5, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <pthread.h>
#include <PeerProto.h>
#include <thread>

namespace hycast {

class PeerProto::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;
    typedef std::exception_ptr      ExceptPtr;
    typedef std::atomic_flag        AtomicFlag;

    AtomicFlag     executing;
    ExceptPtr      exceptPtr;
    mutable Mutex  doneMutex;
    mutable Cond   doneCond;
    TcpSock        noticeSock;   ///< For exchanging notices
    TcpSock        clntSock;     ///< For sending requests and receiving chunks
    TcpSock        srvrSock;     ///< For receiving requests and sending chunks
    std::string    string;       ///< `to_string()` string
    bool           srvrSide;     ///< Server-side construction?
    /// Pool of potential ports for temporary servers
    PortPool       portPool;
    PeerProtoObs* msgRcvr;      ///< Associated receiver of messages
    bool           haltRequested;
    std::thread    noticeThread;
    std::thread    requestThread;
    std::thread    dataThread;

    static const Flags   FLAGS_SEG = 0;  // Data chunk-ID
    static const Flags   FLAGS_PROD = 1; // Product-information chunk-ID
    static const SegSize PAD = 0;        // 16-bit padding

    void handleException(const ExceptPtr& exPtr)
    {
        Guard guard(doneMutex);

        if (!exceptPtr) {
            exceptPtr = exPtr;
            doneCond.notify_one();
        }
    }

    void recvProdNotice(const ProdId prodId)
    {
        LOG_DEBUG("Receiving product notice");
        int    cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            msgRcvr->acceptNotice(prodId);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);
    }

    bool recvSegNotice(const ProdId prodId)
    {
        LOG_DEBUG("Receiving segment notice");
        ProdSize  segOffset;

        if (!noticeSock.read(segOffset))
            return false;

        SegId segId{prodId, segOffset};
        int    cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            msgRcvr->acceptNotice(segId);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    /**
     * Returns on EOF;
     */
    void recvNotices()
    {
        try {
            LOG_DEBUG("Receiving notices");

            for (;;) {
                Flags     flags;
                SegSize   pad;
                ProdId prodId;

                // The following perform network translation
                if (!noticeSock.read(flags) || !noticeSock.read(pad) ||
                        !noticeSock.read(prodId))
                    break; // EOF

                if (flags & FLAGS_PROD) {
                    recvProdNotice(prodId);
                }
                else if (!recvSegNotice(prodId)) {
                    break; // EOF
                }
            }

            halt();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void recvProdRequest(const ProdId prodId)
    {
        int    cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            msgRcvr->acceptRequest(prodId);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);
    }

    bool recvSegRequest(const ProdId prodId)
    {
        ProdSize  segOffset;

        if (!srvrSock.read(segOffset))
            return false;

        SegId id{prodId, segOffset};
        int    cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            msgRcvr->acceptRequest(id);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    /**
     * Returns on EOF;
     */
    void recvRequests()
    {
        try {
            LOG_DEBUG("Receiving requests");

            for (;;) {
                Flags     flags;
                SegSize   pad;
                ProdId prodId;

                // The following perform network translation
                if (!srvrSock.read(flags) || !srvrSock.read(pad) ||
                        !srvrSock.read(prodId))
                    break; // EOF

                if (flags & FLAGS_PROD) {
                    recvProdRequest(prodId);
                }
                else if (!recvSegRequest(prodId)) {
                    break; // EOF
                }
            }

            halt();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    bool recvProdInfo(
            const ProdId prodId,
            const ProdSize  prodSize,
            const SegSize   nameLen)
    {
        char buf[nameLen];

        if (clntSock.read(buf, nameLen) != nameLen)
            return false;

        std::string name(buf, nameLen);
        ProdInfo    prodInfo{prodId, prodSize, name};
        int         cancelState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            msgRcvr->accept(prodInfo);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    bool recvTcpSeg(
            const ProdId prodId,
            const ProdSize  prodSize,
            const SegSize   dataLen)
    {
        ProdSize segOffset;

        if (!clntSock.read(segOffset))
            return false; // EOF

        SegId   id{prodId, segOffset};
        SegInfo info{id, prodSize, dataLen};
        TcpSeg  seg{info, clntSock};
        int     cancelState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            msgRcvr->accept(seg);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    /**
     * Returns on EOF;
     */
    void recvData()
    {
        try {
            LOG_DEBUG("Receiving data");

            for (;;) {
                Flags     flags;
                SegSize   varSize;
                ProdId prodId;
                ProdSize  prodSize;

                // The following perform network translation
                if (!clntSock.read(flags) || !clntSock.read(varSize) ||
                        !clntSock.read(prodId) || ! clntSock.read(prodSize))
                    break; // EOF

                if ((flags & FLAGS_PROD)
                        ? !recvProdInfo(prodId, prodSize, varSize)
                        : !recvTcpSeg(prodId, prodSize, varSize))
                    break; // EOF
            }

            halt();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void startTasks()
    {
        /*
         * The tasks aren't detached because they then couldn't be canceled
         * (`std::thread::native_handle()` returns 0 for detached threads).
         */
        LOG_DEBUG("Creating thread for receiving data");
        dataThread = std::thread(&Impl::recvData, this);

        try {
            LOG_DEBUG("Creating thread for receiving notices");
            noticeThread = std::thread(&Impl::recvNotices, this);

            try {
                LOG_DEBUG("Creating thread for receiving requests");
                requestThread = std::thread(&Impl::recvRequests, this);
            } // `noticeThread` created
            catch (const std::exception& ex) {
                ::pthread_cancel(noticeThread.native_handle());
                throw;
            }
        } // `dataThread` created
        catch (const std::exception& ex) {
            ::pthread_cancel(dataThread.native_handle());
            throw;
        }
    }

    void waitUntilDone()
    {
        Lock lock(doneMutex);

        while (!haltRequested && !exceptPtr)
            doneCond.wait(lock);
    }

    /**
     * Idempotent.
     */
    void stopTasks()
    {
        int status;

        if (requestThread.joinable()) {
            status = ::pthread_cancel(requestThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel request-thread: %s",
                        ::strerror(status));
            requestThread.join();
        }
        if (noticeThread.joinable()) {
            status = ::pthread_cancel(noticeThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel notice-thread: %s",
                        ::strerror(status));
            noticeThread.join();
        }
        if (dataThread.joinable()) {
            status = ::pthread_cancel(dataThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel data-thread: %s",
                        ::strerror(status));
            dataThread.join();
        }
    }

    /**
     * Disconnects from the remote peer. Idempotent.
     */
    void disconnect()
    {
        srvrSock.shutdown();
        clntSock.shutdown();
        noticeSock.shutdown();
    }

    TcpSrvrSock createSrvrSock(
            InetAddr  inAddr,
            PortPool& portPool,
            const int queueSize)
    {
        const int n = portPool.size();
        int       i = 0;

        while (++i <= n) {
            const in_port_t port = portPool.take();
            SockAddr        srvrAddr(inAddr.getSockAddr(port));

            try {
                return TcpSrvrSock(srvrAddr, queueSize);
            }
            catch (const std::exception& ex) {
                log_error(ex);
                portPool.add(port);
            }
        }

        throw RUNTIME_ERROR("Couldn't create server socket");
    }

    void send(
            const ProdId prodId,
            TcpSock&     sock)
    {
        sock.write(FLAGS_PROD); // Performs network translation
        sock.write(PAD);
        sock.write(prodId);
    }

    void send(
            const SegId& id,
            TcpSock&     sock)
    {
        sock.write(FLAGS_SEG);
        sock.write(PAD);
        sock.write(id.getProdId());
        sock.write(id.getSegOffset());
    }

public:
    /**
     * Server-side construction.
     *
     * @param[in] sock      `::accept()`ed TCP socket
     * @param[in] portPool  Pool of potential port numbers for temporary servers
     * @exceptionsafety     Strong guarantee
     * @cancellationpoint   Yes
     */
    Impl(   TcpSock&     sock,
            PortPool&    portPool)
        : executing{false}
        , exceptPtr{}
        , doneMutex{}
        , doneCond{}
        , noticeSock(sock)
        , clntSock{}
        , srvrSock{}
        , string{}
        , srvrSide{true}
        , portPool{portPool}
        , msgRcvr{nullptr}
        , haltRequested{false}
        , noticeThread{}
        , requestThread{}
        , dataThread{}
    {
        InetAddr rmtInAddr{sock.getRmtAddr().getInetAddr()};
        LOG_DEBUG("rmtInAddr: %s", rmtInAddr.to_string().c_str());

        noticeSock.setDelay(false);

        // Create temporary server sockets
        InetAddr    lclInAddr = sock.getLclAddr().getInetAddr();
        TcpSrvrSock srvrSrvrSock{createSrvrSock(lclInAddr, portPool, 1)};

        try {
            TcpSrvrSock clntSrvrSock{createSrvrSock(lclInAddr, portPool, 1)};

            try {
                // Send port numbers of temporary server sockets to remote peer
                noticeSock.write(srvrSrvrSock.getLclPort());
                noticeSock.write(clntSrvrSock.getLclPort());

                // Accept sockets from temporary servers
                // TODO: Ensure connections are from remote peer
                srvrSock = TcpSock{srvrSrvrSock.accept()};
                clntSock = TcpSock{clntSrvrSock.accept()};

                clntSock.setDelay(false);
                srvrSock.setDelay(true); // Consolidate ACK and chunk

                string = "{remote: {addr: " +
                        sock.getRmtAddr().getInetAddr().to_string() +
                        ", ports: [" + std::to_string(sock.getRmtPort()) +
                        ", " + std::to_string(clntSock.getRmtPort()) +
                        ", " + std::to_string(srvrSock.getRmtPort()) +
                        "]}, local: {addr: " +
                        sock.getLclAddr().getInetAddr().to_string() +
                        ", ports: [" + std::to_string(sock.getLclPort()) +
                        ", " + std::to_string(clntSock.getLclPort()) +
                        ", " + std::to_string(srvrSock.getLclPort()) +
                        "]}}";
            }
            catch (...) {
                portPool.add(clntSrvrSock.getLclPort());
                throw;
            }
        }
        catch (...) {
            portPool.add(srvrSrvrSock.getLclPort());
            throw;
        }
    }

    /**
     * Client-side construction.
     *
     * @param[in] rmtSrvrAddr         Socket address of the remote peer-server
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @exceptionsafety               Strong guarantee
     * @cancellationpoint             Yes
     */
    Impl(const SockAddr& rmtSrvrAddr)
        : executing{false}
        , exceptPtr{}
        , doneMutex{}
        , doneCond{}
        , noticeSock{TcpClntSock(rmtSrvrAddr)}
        , clntSock{}
        , srvrSock{}
        , string()
        , srvrSide{false}
        , msgRcvr{nullptr}
        , haltRequested{false}
        , noticeThread{}
        , requestThread{}
        , dataThread{}
    {
        noticeSock.setDelay(false);

        // Read port numbers of remote peer's temporary servers
        in_port_t rmtSrvrPort, rmtClntPort;
        noticeSock.read(rmtSrvrPort);
        noticeSock.read(rmtClntPort);

        // Create sockets to remote peer's temporary servers
        clntSock = TcpClntSock(rmtSrvrAddr.clone(rmtSrvrPort));
        srvrSock = TcpClntSock(rmtSrvrAddr.clone(rmtClntPort));

        clntSock.setDelay(false);
        srvrSock.setDelay(true); // Consolidate ACK and chunk

        string = "{remote: {addr: " +
                noticeSock.getRmtAddr().getInetAddr().to_string() +
                ", ports: [" + std::to_string(noticeSock.getRmtPort()) +
                ", " + std::to_string(clntSock.getRmtPort()) +
                ", " + std::to_string(srvrSock.getRmtPort()) +
                "]}, local: {addr: " +
                noticeSock.getLclAddr().getInetAddr().to_string() +
                ", ports: [" + std::to_string(noticeSock.getLclPort()) +
                ", " + std::to_string(clntSock.getLclPort()) +
                ", " + std::to_string(srvrSock.getLclPort()) +
                "]}}";
    }

    ~Impl()
    {
        stopTasks(); // Idempotent
        disconnect(); // Idempotent
        if (srvrSide) {
            portPool.add(clntSock.getLclPort());
            portPool.add(srvrSock.getLclPort());
        }
    }

    Impl& set(PeerProtoObs* const msgRcvr)
    {
        this->msgRcvr = msgRcvr;
        return *this;
    }

    SockAddr getRmtAddr() const
    {
        return noticeSock.getRmtAddr();
    }

    SockAddr getLclAddr() const
    {
        return noticeSock.getLclAddr();
    }

    std::string to_string() const
    {
        return "{rmtAddr: " + getRmtAddr().to_string() + ", lclAddr: " +
                getLclAddr().to_string() + "}";
    }

    /**
     * Executes this instance by starting subtasks. Doesn't return until one of
     * the following:
     *   - The remote peer closes the connection;
     *   - `halt()` is called;
     *   - An exception is thrown by a subtask.
     * Upon return, all subtasks have terminated and the remote peer will have
     * been disconnected. If `halt()` is called before this method, then this
     * instance will return immediately and won't execute.
     *
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @throws    std::logic_error    This method has already been called
     */
    void operator ()()
    {
        if (executing.test_and_set())
            throw LOGIC_ERROR("Already called");
        if (msgRcvr == nullptr)
            throw LOGIC_ERROR("Message receiver not set");

        startTasks();

        try {
            waitUntilDone();
            stopTasks(); // Idempotent
            disconnect(); // Idempotent

            Guard guard{doneMutex};
            if (!haltRequested && exceptPtr)
                std::rethrow_exception(exceptPtr);
        }
        catch (...) {
            stopTasks(); // Idempotent
            disconnect(); // Idempotent
            throw;
        }
    }

    /**
     * Halts execution. Does nothing if `operator()()` has not been called;
     * otherwise, causes `operator()()` to return. Idempotent.
     *
     * @cancellationpoint No
     */
    void halt() noexcept
    {
        Guard guard(doneMutex);

        haltRequested = true;
        doneCond.notify_one();
    }

    void notify(const ProdId prodId)
    {
        send(prodId, noticeSock);
    }

    void notify(const SegId& id)
    {
        send(id, noticeSock);
    }

    void request(const ProdId prodId)
    {
        send(prodId, clntSock);
    }

    void request(const SegId segId)
    {
        send(segId, clntSock);
    }

    void send(const ProdInfo& info)
    {
        const std::string& name = info.getName();
        const SegSize      nameLen = name.length();

        // The following perform host-to-network translation
        srvrSock.write(FLAGS_PROD);
        srvrSock.write(nameLen);
        srvrSock.write(info.getIndex());
        srvrSock.write(info.getSize());

        srvrSock.write(name.data(), nameLen); // No network translation
    }

    void send(const MemSeg& seg)
    {
        const SegInfo& info = seg.getInfo();
        SegSize        segSize = info.getSegSize();

        // The following perform host-to-network translation
        srvrSock.write(FLAGS_SEG);
        srvrSock.write(segSize);
        srvrSock.write(info.getId().getProdId());
        srvrSock.write(info.getProdSize());
        srvrSock.write(info.getId().getSegOffset());

        srvrSock.write(seg.getData(), segSize); // No network translation
    }
};

PeerProto::PeerProto(Impl* impl)
    : pImpl{impl}
{}

PeerProto::PeerProto(
        TcpSock&     sock,
        PortPool&    portPool)
    : PeerProto{new Impl(sock, portPool)}
{}

PeerProto::PeerProto(const SockAddr& rmtSrvrAddr)
    : PeerProto{new Impl(rmtSrvrAddr)}
{}

const PeerProto& PeerProto::set(PeerProtoObs* const msgRcvr) const
{
    pImpl->set(msgRcvr);
    return *this;
}

SockAddr PeerProto::getRmtAddr() const
{
    return pImpl->getRmtAddr();
}

SockAddr PeerProto::getLclAddr() const
{
    return pImpl->getLclAddr();
}

std::string PeerProto::to_string() const
{
    return pImpl->to_string();
}

void PeerProto::operator()() const
{
    pImpl->operator()();
}

void PeerProto::halt() const
{
    pImpl->halt();
}

void PeerProto::notify(const ProdId prodId) const
{
    pImpl->notify(prodId);
}

void PeerProto::notify(const SegId& id) const
{
    pImpl->notify(id);
}

void PeerProto::request(const ProdId prodId) const
{
    pImpl->request(prodId);
}

void PeerProto::request(const SegId segId) const
{
    pImpl->request(segId);
}

void PeerProto::send(const ProdInfo& prodInfo) const
{
    pImpl->send(prodInfo);
}

void PeerProto::send(const MemSeg& memSeg) const
{
    pImpl->send(memSeg);
}

} // namespace

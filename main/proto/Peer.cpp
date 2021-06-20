/**
 * This file defines the Peer class. The Peer class handles low-level,
 * bidirectional messaging with its remote counterpart.
 *
 *  @file:  Peer.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#include "logging.h"
#include "Peer.h"
#include "ThreadException.h"

#include <atomic>
#include <list>
#include <queue>
#include <unordered_map>
#include <utility>

namespace hycast {

class Peer::Impl
{
protected:
    mutable Mutex     mutex;
    mutable Mutex     rmtSockAddrMutex;
    P2pNode&          node;
    /*
     * If a single socket is used for asynchronous communication and reading and
     * writing occur on the same thread, then deadlock will occur if both
     * receive buffers are full and each end is trying to write. To prevent
     * this, three sockets are used and a thread that reads from one socket will
     * write to another. The pipeline might block for a while, but it won't
     * deadlock.
     */
    TcpSock           noticeSock;
    TcpSock           requestSock;
    TcpSock           dataSock;
    Thread            noticeThread;
    Thread            requestThread;
    Thread            dataThread;
    SockAddr          rmtSockAddr;
    std::atomic<bool> rmtPubPath;
    enum class State {
        CONSTRUCTED,
        STARTED,
        STOPPING
    }                 state;
    const bool        clientSide;

    /**
     * Orders the sockets so that the notice socket has the lowest client-side
     * port number, then the request socket, and then the data socket.
     */
    void orderSocks() {
        if (clientSide) {
            if (requestSock.getLclPort() < noticeSock.getLclPort())
                requestSock.swap(noticeSock);

            if (dataSock.getLclPort() < requestSock.getLclPort()) {
                dataSock.swap(requestSock);
                if (requestSock.getLclPort() < noticeSock.getLclPort())
                    requestSock.swap(noticeSock);
            }
        }
        else {
            if (requestSock.getRmtPort() < noticeSock.getRmtPort())
                requestSock.swap(noticeSock);

            if (dataSock.getRmtPort() < requestSock.getRmtPort()) {
                dataSock.swap(requestSock);
                if (requestSock.getRmtPort() < noticeSock.getRmtPort())
                    requestSock.swap(noticeSock);
            }
        }
    }

    /**
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool connectClient() {
        bool     success = false;
        SockAddr rmtSock;

        {
            Guard guard{rmtSockAddrMutex};
            rmtSock = rmtSockAddr;
        }

        // Connect to Peer server.
        // Keep consonant with `PeerSrvr::accept()`
        noticeSock = TcpClntSock(rmtSock);

        try {
            const in_port_t noticePort = noticeSock.getLclPort();

            if (noticeSock.write(noticePort)) {
                requestSock = TcpClntSock(rmtSock);

                try {
                    if (requestSock.write(noticePort)) {
                        dataSock = TcpClntSock(rmtSock);

                        try {
                            if (!dataSock.write(noticePort)) {
                                dataSock.close();
                            }
                            else {
                                orderSocks();
                                success = true;
                            }
                        } // `dataSock` open
                        catch (const std::exception& ex) {
                            dataSock.close();
                            throw;
                        }
                    }

                    if (!success)
                        requestSock.close();
                } // `requestSock` open
                catch (const std::exception& ex) {
                    requestSock.close();
                    throw;
                }
            }

            if (!success)
                noticeSock.close();
        } // `noticeSock` open
        catch (const std::exception& ex) {
            noticeSock.close();
            throw;
        }

        return success;
    }

    void startThreads(Peer peer) {
        noticeThread = Thread(&Impl::run, this, noticeSock, peer);

        try {
            requestThread = Thread(&Impl::run, this, requestSock, peer);

            try {
                dataThread = Thread(&Impl::run, this, dataSock, peer);
                noticeThread.detach();
                requestThread.detach();
                dataThread.detach();
            } // `requestThread` created
            catch (const std::exception& ex) {
                ::pthread_cancel(requestThread.native_handle());
                requestThread.join();
                throw;
            }
        }
        catch (const std::exception& ex) {
            ::pthread_cancel(noticeThread.native_handle());
            noticeThread.join();
            throw;
        } // `noticeThread` created
    }

    /**
     * Idempotent.
     */
    void stopThreads() {
        LOG_TRACE;
        dataSock.shutdown(SHUT_RD);
        requestSock.shutdown(SHUT_RD);
        noticeSock.shutdown(SHUT_RD);
        LOG_TRACE;
    }

    void joinThreads() {
        LOG_TRACE;
        if (dataThread.joinable())
            dataThread.join();
        LOG_TRACE;
        if (requestThread.joinable())
            requestThread.join();
        LOG_TRACE;
        if (noticeThread.joinable())
            noticeThread.join();
        LOG_TRACE;
    }

    static inline bool write(TcpSock& sock, PduId id) {
        LOG_TRACE;
        return sock.write(static_cast<PduType>(id));
    }

    static inline bool read(TcpSock& sock, PduId& pduId) {
        PduType id;
        auto    success = sock.read(id);
        pduId = static_cast<PduId>(id);
        return success;
    }

    static inline bool write(TcpSock& sock, const ProdIndex index) {
        LOG_TRACE;
        return sock.write((ProdIndex::Type)index);
    }

    static inline bool read(TcpSock& sock, ProdIndex& index) {
        ProdIndex::Type i;
        if (sock.read(i)) {
            index = ProdIndex(i);
            return true;
        }
        return false;
    }

    static inline bool write(TcpSock& sock, const Timestamp& timestamp) {
        LOG_TRACE;
        return sock.write(timestamp.sec) && sock.write(timestamp.nsec);
    }

    static inline bool read(TcpSock& sock, Timestamp& timestamp) {
        LOG_TRACE;
        return sock.read(timestamp.sec) && sock.read(timestamp.nsec);
    }

    static inline bool write(TcpSock& sock, const DataSegId& id) {
        return write(sock, id.prodIndex) && sock.write(id.offset);
    }

    static inline bool write(TcpSock& sock, const ProdInfo& prodInfo) {
        return write(sock, prodInfo.getProdIndex()) &&
                sock.write(prodInfo.getName()) &&
                sock.write(prodInfo.getProdSize()) &&
                write(sock, prodInfo.getTimestamp());
    }

    static bool read(TcpSock& sock, ProdInfo& prodInfo) {
        ProdIndex index;
        String    name;
        ProdSize  size;
        Timestamp timestamp;

        if (!read(sock, index) ||
               !sock.read(name) ||
               !sock.read(size) ||
               !read(sock, timestamp))
            return false;

        prodInfo = ProdInfo(index, name, size, timestamp);
        return true;
    }

    static inline bool write(TcpSock& sock, const DataSeg& dataSeg) {
        return write(sock, dataSeg.segId()) &&
                sock.write(dataSeg.prodSize()) &&
                sock.write(dataSeg.data(), dataSeg.size());
    }

    static inline bool read(TcpSock& sock, DataSegId& id) {
        return read(sock, id.prodIndex) && sock.read(id.offset);
    }

    static inline bool read(TcpSock& sock, bool& value) {
        return sock.read(value);
    }

    static bool read(TcpSock& sock, DataSeg& dataSeg) {
        bool success = false;
        DataSegId id;
        ProdSize  size;
        if (read(sock, id) && sock.read(size)) {
            dataSeg = DataSeg(id, size, sock);
            success = true;;
        }
        return success;
    }

    /**
     * Dispatch function for processing an incoming message from the remote
     * peer.
     *
     * @param[in] id            Message type
     * @param[in] peer          Local peer associated with remote peer
     * @retval    `false`       End-of-file encountered.
     * @retval    `true`        Success
     * @throw std::logic_error  `id` is unknown
     */
    bool processPdu(const PduId id, Peer& peer) {
        bool success = false;
        int  cancelState;

        switch (id) {
        case PduId::PUB_PATH_NOTICE: {
            LOG_TRACE;
            bool notice;
            if (read(noticeSock, notice)) {
                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                node.recvNotice(PubPath(notice), peer);
                ::pthread_setcancelstate(cancelState, &cancelState);
                rmtPubPath = notice;
                success = true;
            }
            break;
        }
        case PduId::PROD_INFO_NOTICE: {
            LOG_TRACE;
            ProdIndex notice;
            if (read(noticeSock, notice)) {
                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                node.recvNotice(notice, peer);
                ::pthread_setcancelstate(cancelState, &cancelState);
                success = true;
            }
            break;
        }
        case PduId::DATA_SEG_NOTICE: {
            LOG_TRACE;
            DataSegId notice;
            if (read(noticeSock, notice)) {
                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                node.recvNotice(notice, peer);
                ::pthread_setcancelstate(cancelState, &cancelState);
                success = true;
            }
            break;
        }
        case PduId::PROD_INFO_REQUEST: {
            LOG_TRACE;
            ProdIndex request;
            if (read(requestSock, request)) {
                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                node.recvRequest(request, peer);
                ::pthread_setcancelstate(cancelState, &cancelState);
                success = true;
            }
            break;
        }
        case PduId::DATA_SEG_REQUEST: {
            LOG_TRACE;
            DataSegId request;
            if (read(requestSock, request)) {
                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                node.recvRequest(request, peer);
                ::pthread_setcancelstate(cancelState, &cancelState);
                success = true;
            }
            break;
        }
        case PduId::PROD_INFO: {
            LOG_TRACE;
            ProdInfo data;
            if (read(dataSock, data)) {
                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                node.recvData(data, peer);
                ::pthread_setcancelstate(cancelState, &cancelState);
                success = true;
            }
            break;
        }
        case PduId::DATA_SEG: {
            LOG_TRACE;
            DataSeg dataSeg;
            if (read(dataSock, dataSeg)) {
                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                node.recvData(dataSeg, peer);
                ::pthread_setcancelstate(cancelState, &cancelState);
                success = true;
            }
            break;
        }
        default:
            throw std::logic_error("Invalid PDU type: " +
                    std::to_string(static_cast<PduType>(id)));
        }

        return success;
    }

    /**
     * Reads one socket from the remote peer and processes incoming messages.
     * Doesn't return until either EOF is encountered or an error occurs.
     *
     * @param[in] sock    Socket with remote peer
     * @param[in] peer    Associated local peer
     * @throw LogicError  Message type is unknown
     */
    void run(TcpSock sock,
             Peer    peer) {
        try {
            for (;;) {
                PduId id;
                if (!read(sock, id) || !processPdu(id, peer))
                    break; // EOF
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }

        {
            Guard guard{mutex};
            state = State::STOPPING;
        }
    }

public:
    /**
     * Constructs.
     *
     * @param[in] node      P2P node
     * @param[in] srvrAddr  Socket address of remote P2P server. Must be
     *                      invalid if server-side constructed.
     */
    Impl(P2pNode& node, const SockAddr& srvrAddr)
        : mutex()
        , node(node)
        , noticeSock()
        , requestSock()
        , dataSock()
        , noticeThread()
        , requestThread()
        , dataThread()
        , rmtSockAddr(srvrAddr)
        , rmtPubPath(false)
        , state(State::CONSTRUCTED)
        , clientSide(static_cast<bool>(srvrAddr))
    {}

    /**
     * Server-side construction.
     *
     * @param[in] node  P2P node
     */
    explicit Impl(P2pNode& node)
        : Impl(node, SockAddr{})
    {}

    Impl(const Impl& impl) =delete; // Rule of three

    ~Impl() noexcept {
        LOG_TRACE;
        Guard guard{mutex};
        if (state == State::STARTED)
            LOG_ERROR("Peer wasn't stopped");
        try {
            stopThreads(); // Idempotent
            //joinThreads();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    Impl& operator=(const Impl& rhs) noexcept =delete; // Rule of three

    /**
     * Sets the next, individual socket. Server-side only.
     *
     * @param[in] sock        Relevant socket
     * @throw     LogicError  Connection is already complete
     */
    void set(TcpSock& sock) {
        if (clientSide)
            throw LOGIC_ERROR("Can't set client-side socket");

        Guard guard{mutex};

        // NB: Keep function consonant with `Impl(SockAddr)`

        if (!noticeSock) {
            noticeSock = sock;
            Guard guard{rmtSockAddrMutex};
            rmtSockAddr = noticeSock.getRmtAddr();
        }
        else if (!requestSock) {
            requestSock = sock;
        }
        else if (!dataSock) {
            dataSock = sock;
            orderSocks();
        }
        else {
            throw LOGIC_ERROR("Server-side P2P connection is complete");
        }
    }

    /**
     * Indicates if instance is complete (i.e., has all individual sockets).
     *
     * @retval `false`  Instance is not complete
     * @retval `true`   Instance is complete
     */
    bool isComplete() const noexcept {
        Guard guard{mutex};
        return noticeSock && requestSock && dataSock;
    }

    /**
     * Starts this instance. Does the following:
     *   - If client-side constructed, blocks while connecting to the remote
     *     peer
     *   - Creates threads on which
     *       - The sockets are read; and
     *       - The P2P node is called.
     *
     * @param[in] peer     Associated peer
     * @retval    `false`  Remote peer disconnected
     * @retval    `true`   Success
     * @throw LogicError   Already started
     * @throw SystemError  Thread couldn't be created
     * @see   `stop()`
     */
    bool start(Peer& peer) {
        LOG_TRACE;
        Guard guard{mutex};

        if (state != State::CONSTRUCTED)
            throw LOGIC_ERROR("Peer wasn't just constructed");

        bool success = clientSide
                ? connectClient()
                : true;

        if (success) {
            startThreads(peer);
            state = State::STARTED;
            // `stop()` is now effective
        }

        return success;
    }

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() const noexcept {
        Guard guard{rmtSockAddrMutex};
        return rmtSockAddr;
    }

    /**
     * Stops this instance from serving its remote counterpart.
     *   - Cancels the thread on which the remote peer is being served
     *   - Joins with that thread
     *
     * @throw LogicError  Peer wasn't started
     * @see   `start()`
     */
    void stop() {
        LOG_TRACE;
        {
            Guard guard{mutex};
            LOG_TRACE;
            if (state != State::STOPPING) {
                if (state != State::STARTED)
                    throw LOGIC_ERROR("Peer wasn't started");
                stopThreads();
                state = State::STOPPING;
            }
        }
        LOG_TRACE;
    }

    String to_string(const bool withName) const {
        Guard guard{rmtSockAddrMutex};
        return rmtSockAddr.to_string(withName);
    }

    /**
     * Notifies the remote peer.
     *
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool notify(const PubPath notice) {
        return write(noticeSock, PduId::PUB_PATH_NOTICE) &&
                noticeSock.write(notice.operator bool());
    }
    bool notify(const ProdIndex notice) {
        LOG_TRACE;
        return write(noticeSock, PduId::PROD_INFO_NOTICE) &&
            write(noticeSock, notice);
    }
    bool notify(const DataSegId& notice) {
        LOG_TRACE;
        return write(noticeSock, PduId::DATA_SEG_NOTICE) &&
            write(noticeSock, notice);
    }

    /**
     * Requests data from the remote peer.
     *
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool request(const ProdIndex request) {
        return write(requestSock, PduId::PROD_INFO_REQUEST) &&
                write(requestSock, request);
    }
    bool request(const DataSegId& request) {
        return write(requestSock, PduId::DATA_SEG_REQUEST) &&
            write(requestSock, request);
    }

    /**
     * Sends data to the remote peer.
     *
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool send(const ProdInfo& data) {
        return write(dataSock, PduId::PROD_INFO) &&
                write(dataSock, data);
    }
    bool send(const DataSeg& data) {
        write(dataSock, PduId::DATA_SEG) &&
                write(dataSock, data);
    }

    bool rmtIsPubPath() const noexcept {
        return rmtPubPath;
    }
};

/******************************************************************************/

Peer::Peer(SharedPtr& pImpl)
    : pImpl(pImpl)
{}

Peer::Peer(P2pNode& node)
    /*
     * Passing `this` or `*this` to the `Impl` ctor doesn't make changes to
     * `pImpl` visible: `pImpl` isn't visible while `Impl` is being constructed.
     */
    : pImpl(std::make_shared<Impl>(node))
{
    LOG_TRACE;
}

Peer::Peer(P2pNode& node, const SockAddr& srvrAddr)
    : pImpl(std::make_shared<Impl>(node, srvrAddr))
{
    LOG_TRACE;
}

Peer& Peer::set(TcpSock& sock) {
    pImpl->set(sock);
    return *this;
}

bool Peer::isComplete() const noexcept {
    return pImpl->isComplete();
}

bool Peer::start() {
    return pImpl->start(*this);
}

SockAddr Peer::getRmtAddr() noexcept {
    return pImpl->getRmtAddr();
}

void Peer::stop() {
    pImpl->stop();
}

Peer::operator bool() const {
    return static_cast<bool>(pImpl);
}

size_t Peer::hash() const noexcept {
    /*
     * The underlying pointer is used instead of `pImpl->hash()` because
     * hashing a client-side socket in the implementation won't work until
     * `connect()` returns -- which could be a while -- and `PeerSet`, at least,
     * requires a hash before it calls `Peer::start()`.
     *
     * This means, however, that it's possible to have multiple client-side
     * peers connected to the same remote host within the same process.
     */
    return std::hash<Impl*>()(pImpl.get());
}

bool Peer::operator<(const Peer rhs) const noexcept {
    // Must be consistent with `hash()`
    return pImpl.get() < rhs.pImpl.get();
}

bool Peer::operator==(const Peer& rhs) const noexcept {
    return !(*this < rhs) && !(rhs < *this);
}

String Peer::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

bool Peer::notify(const PubPath notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const ProdIndex notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const DataSegId& notice) const {
    return pImpl->notify(notice);
}

bool Peer::request(const ProdIndex request) const {
    return pImpl->request(request);
}

bool Peer::request(const DataSegId& request) const {
    return pImpl->request(request);
}

bool Peer::send(const ProdInfo& data) const {
    return pImpl->send(data);
}

bool Peer::send(const DataSeg& data) const {
    return pImpl->send(data);
}

bool Peer::rmtIsPubPath() const noexcept {
    pImpl->rmtIsPubPath();
}

/******************************************************************************/

/**
 * Peer server implementation.
 */
class PeerSrvr::Impl
{
    class PeerFactory
    {
        struct Hash {
            size_t operator()(const SockAddr& sockAddr) const {
                return sockAddr.hash();
            };
        };

        struct areEqual {
            bool operator()(const SockAddr& lhs, const SockAddr& rhs) const {
                return lhs == rhs;
            };
        };

        using Map = std::unordered_map<SockAddr, Peer, Hash, areEqual>;

        P2pNode& node;
        Map      peers;

    public:
        PeerFactory(P2pNode& node)
            : node(node)
            , peers()
        {}

        /**
         * Adds an individual socket to a peer. If the addition completes the
         * peer, then it is removed from this instance.
         *
         * @param[in] sock  Individual socket
         * @return          Corresponding peer. `Peer::isComplete()` is true,
         *                  then the peer has been removed from this instance.
         */
        Peer add(TcpSock& sock, in_port_t noticePort) {
            // TODO: Limit number of outstanding connections
            // TODO: Purge old entries
            auto key = sock.getRmtAddr().clone(noticePort);
            auto peer = peers[key];

            if (!peer)
                peers[key] = peer = Peer{node}; // Entry was default constructed

            if (peer.set(sock).isComplete())
                peers.erase(key);

            return peer;
        }
    };

    using PeerQ = std::queue<Peer, std::list<Peer>>;

    mutable Mutex     mutex;
    mutable Cond      cond;
    PeerFactory       peerFactory;
    const SockAddr    srvrAddr;
    const TcpSrvrSock srvrSock;
    PeerQ             acceptQ;
    PeerQ::size_type  maxAccept;

    /**
     * Executes on separate thread.
     *
     * @param[in] sock  Newly-accepted socket
     */
    void acceptSock(TcpSock sock) {
        in_port_t noticePort;

        if (sock.read(noticePort)) { // Might take a while
            // The rest is fast
            Guard guard{mutex};

            if (acceptQ.size() < maxAccept) {
                auto peer = peerFactory.add(sock, noticePort);

                if (peer.isComplete())
                    acceptQ.push(peer);

                cond.notify_one();
            }
        }
    }

public:
    /**
     * Constructs from the local address of the server.
     *
     * @param[in] node       P2P node
     * @param[in] srvrAddr   Local Address of P2P server
     * @param[in] maxAccept  Maximum number of outstanding P2P connections
     */
    Impl(   P2pNode&        node,
            const SockAddr& srvrAddr,
            const unsigned  maxAccept)
        : mutex()
        , cond()
        , peerFactory(node)
        , srvrAddr(srvrAddr)
        , srvrSock(srvrAddr, 3*maxAccept)
        , acceptQ()
        , maxAccept(maxAccept)
    {}

    /**
     * Returns the next, accepted, peer-to-peer connection.
     *
     * @return Next P2P connection
     */
    Peer accept() {
        Lock lock{mutex};

        while (acceptQ.empty()) {
            // TODO: Limit number of threads
            // TODO: Lower priority of thread to favor data transmission
            Thread(&Impl::acceptSock, this, srvrSock.accept()).detach();
            cond.wait(lock);
        }

        auto peer = acceptQ.front();
        acceptQ.pop();

        return peer;
    }
};

PeerSrvr::PeerSrvr(P2pNode&        node,
                   const SockAddr& srvrAddr,
                   const unsigned  maxAccept)
    : pImpl(std::make_shared<Impl>(node, srvrAddr, maxAccept))
{}

Peer PeerSrvr::accept() {
    return pImpl->accept();
}

} // namespace

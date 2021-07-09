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
    mutable Mutex      sockMutex;
    mutable Mutex      rmtSockAddrMutex;
    mutable Mutex      exceptMutex;
    P2pNode&           node;
    /*
     * If a single socket is used for asynchronous communication and reading and
     * writing occur on the same thread, then deadlock will occur if both
     * receive buffers are full and each end is trying to write. To prevent
     * this, three sockets are used and a thread that reads from one socket will
     * write to another. The pipeline might block for a while, but it won't
     * deadlock.
     */
    TcpSock            noticeSock;
    TcpSock            requestSock;
    TcpSock            dataSock;
    Thread             noticeReader;
    Thread             requestReader;
    Thread             dataReader;
    Thread             requestWriter;
    SockAddr           rmtSockAddr;
    std::atomic<bool>  rmtPubPath;
    enum class State {
        INITED,
        STARTING,
        STARTED,
        STOPPING
    };
    using AtomicState = std::atomic<State>;
    AtomicState        state;
    const bool         clientSide;
    std::exception_ptr exPtr;
    RequestQueue       requestQ;

    /**
     * Orders the sockets so that the notice socket has the lowest client-side
     * port number, then the request socket, and then the data socket.
     *
     * @param[in] notSock  Notice socket
     * @param[in] reqSock  Request socket
     * @param[in] datSock  Data socket
     */
    void orderSocks(TcpSock& notSock,
                    TcpSock& reqSock,
                    TcpSock& datSock) {
        if (clientSide) {
            if (reqSock.getLclPort() < notSock.getLclPort())
                reqSock.swap(notSock);

            if (datSock.getLclPort() < reqSock.getLclPort()) {
                datSock.swap(reqSock);
                if (reqSock.getLclPort() < notSock.getLclPort())
                    reqSock.swap(notSock);
            }
        }
        else {
            if (reqSock.getRmtPort() < notSock.getRmtPort())
                reqSock.swap(notSock);

            if (datSock.getRmtPort() < reqSock.getRmtPort()) {
                datSock.swap(reqSock);
                if (reqSock.getRmtPort() < notSock.getRmtPort())
                    reqSock.swap(notSock);
            }
        }
    }

    /**
     * Connects a client-side peer to a remote peer. Blocks while connecting.
     *
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool connectClient() {
        bool     success = false;
        SockAddr srvrAddr;

        {
            Guard guard{rmtSockAddrMutex};
            srvrAddr = rmtSockAddr;
        }

        // Connect to Peer server.
        // Keep consonant with `PeerSrvr::accept()`
        TcpClntSock notSock{srvrAddr};

        const in_port_t noticePort = notSock.getLclPort();

        if (notSock.write(noticePort)) {
            TcpClntSock reqSock{srvrAddr};

            if (reqSock.write(noticePort)) {
                TcpClntSock datSock{srvrAddr};

                if (datSock.write(noticePort)) {
                    // Use of local RAII sockets => sockets close on error
                    orderSocks(notSock, reqSock, datSock); // Might throw
                    noticeSock  = notSock;
                    requestSock = reqSock;
                    dataSock    = datSock;
                    success = true;
                }
            }
        }

        return success;
    }

    void startThreads() {
        noticeReader = Thread(&Impl::runReader, this, noticeSock);

        try {
            requestReader = Thread(&Impl::runReader, this, requestSock);

            try {
                dataReader = Thread(&Impl::runReader, this, dataSock);

                try {
                    requestWriter = Thread(&Impl::runRequester, this);
                } // `dataReader` created
                catch (const std::exception& ex) {
                    ::pthread_cancel(dataReader.native_handle());
                    dataReader.join();
                    throw;
                }
            } // `requestReader` created
            catch (const std::exception& ex) {
                ::pthread_cancel(requestReader.native_handle());
                requestReader.join();
                throw;
            }
        } // `noticeReader` created
        catch (const std::exception& ex) {
            ::pthread_cancel(noticeReader.native_handle());
            noticeReader.join();
            throw;
        }
    }

    /**
     * Idempotent.
     */
    void stopThreads() {
        LOG_TRACE;
        if (dataSock)
            dataSock.shutdown(SHUT_RD);
        if (requestSock)
            requestSock.shutdown(SHUT_RD);
        if (noticeSock)
            noticeSock.shutdown(SHUT_RD);
        LOG_TRACE;
    }

    void joinThreads() {
        LOG_TRACE;
        if (dataReader.joinable())
            dataReader.join();
        LOG_TRACE;
        if (requestReader.joinable())
            requestReader.join();
        LOG_TRACE;
        if (noticeReader.joinable())
            noticeReader.join();
        LOG_TRACE;
    }

    static inline bool write(TcpSock& sock, const PduId id) {
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
     * @retval    `false`       End-of-file encountered.
     * @retval    `true`        Success
     * @throw std::logic_error  `id` is unknown
     */
    bool processPdu(const PduId id) {
        bool success = false;
        int  cancelState;

        switch (id) {
        case PduId::PUB_PATH_NOTICE: {
            LOG_TRACE;
            bool notice;
            if (read(noticeSock, notice)) {
                node.recvNotice(PubPath(notice), rmtSockAddr);
                rmtPubPath = notice;
                success = true;
            }
            break;
        }
        case PduId::PROD_INFO_NOTICE: {
            LOG_TRACE;
            ProdIndex notice;
            if (read(noticeSock, notice) &&
                    node.recvNotice(notice, rmtSockAddr))
                success = request(notice);
            break;
        }
        case PduId::DATA_SEG_NOTICE: {
            LOG_TRACE;
            DataSegId notice;
            if (read(noticeSock, notice) &&
                    node.recvNotice(notice, rmtSockAddr))
                success = request(notice);
            break;
        }
        case PduId::PROD_INFO_REQUEST: {
            LOG_TRACE;
            ProdIndex request;
            if (read(requestSock, request)) {
                auto prodInfo = node.recvRequest(request, rmtSockAddr);
                success = prodInfo && send(prodInfo);
            }
            break;
        }
        case PduId::DATA_SEG_REQUEST: {
            LOG_TRACE;
            DataSegId request;
            if (read(requestSock, request)) {
                auto dataSeg = node.recvRequest(request, rmtSockAddr);
                success = dataSeg && send(dataSeg);
            }
            break;
        }
        case PduId::PROD_INFO: {
            LOG_TRACE;
            ProdInfo data;
            if (read(dataSock, data)) {
                node.recvData(data, rmtSockAddr);
                success = true;
            }
            break;
        }
        case PduId::DATA_SEG: {
            LOG_TRACE;
            DataSeg dataSeg;
            if (read(dataSock, dataSeg)) {
                node.recvData(dataSeg, rmtSockAddr);
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

    void setExPtr() {
        Guard guard{exceptMutex};
        if (!exPtr)
            exPtr = std::current_exception();
    }

    void throwIfExPtr() {
        bool throwEx = false;
        {
            Guard guard{exceptMutex};
            throwEx = static_cast<bool>(exPtr);
        }
        if (throwEx)
            std::rethrow_exception(exPtr);
    }

    /**
     * Reads one socket from the remote peer and processes incoming messages.
     * Doesn't return until either EOF is encountered or an error occurs.
     *
     * @param[in] sock    Socket with remote peer
     * @throw LogicError  Message type is unknown
     */
    void runReader(TcpSock sock) {
        try {
            for (;;) {
                PduId id;
                if (!read(sock, id) || !processPdu(id))
                    break; // EOF
            }
        }
        catch (const std::exception& ex) {
            setExPtr();
        }
    }

    void runRequester() {
        try {
            for (;;)
                requestQ.request(*this);
        }
        catch (const std::exception& ex) {
            setExPtr();
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
        : sockMutex()
        , rmtSockAddrMutex()
        , exceptMutex()
        , node(node)
        , noticeSock()
        , requestSock()
        , dataSock()
        , noticeReader()
        , requestReader()
        , dataReader()
        , requestWriter()
        , rmtSockAddr(srvrAddr)
        , rmtPubPath(false)
        , state(State::INITED)
        , clientSide(static_cast<bool>(srvrAddr))
        , exPtr()
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
        try {
            stop(); // Idempotent
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

        Guard guard{sockMutex};

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
            orderSocks(noticeSock, requestSock, dataSock);
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
        Guard guard{sockMutex};
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
     * @retval    `false`  Peer is client-side and couldn't connect with remote
     *                     peer
     * @retval    `false`  `stop()` was called
     * @retval    `true`   Success
     * @throw LogicError   Already called
     * @throw SystemError  Thread couldn't be created
     * @see   `stop()`
     */
    bool start() {
        LOG_TRACE;
        bool  success;
        State lclState{State::INITED};

        if (!state.compare_exchange_strong(lclState, State::STARTING)) {
            if (lclState == State::STARTING || lclState == State::STARTED)
                throw LOGIC_ERROR("start() already called");
        }
        else {
            success = clientSide
                    ? connectClient()
                    : true;

            if (success) {
                startThreads();
                lclState = State::STARTING;
                if (!state.compare_exchange_strong(lclState, State::STARTED)) {
                    stopThreads();
                    joinThreads();
                    success = false;
                }
                // `stop()` is now effective
            }
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
     * Stops this instance from serving its remote counterpart. Causes the
     * threads serving the remote peer to terminate. If called before `start()`,
     * then the remote peer will not be served.
     *
     * Idempotent.
     *
     * @see   `start()`
     */
    void stop() {
        LOG_TRACE;
        State expected = State::INITED;

        if (!state.compare_exchange_strong(expected, State::STOPPING)) {
            expected = State::STARTING;

            if (!state.compare_exchange_strong(expected, State::STOPPING)) {
                expected = State::STARTED;

                if (state.compare_exchange_strong(expected, State::STOPPING)) {
                    stopThreads();
                    joinThreads();
                }
            }
        }
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
        throwIfExPtr();
        return write(noticeSock, PduId::PUB_PATH_NOTICE) &&
                noticeSock.write(notice.operator bool());
    }
    bool notify(const ProdIndex notice) {
        LOG_TRACE;
        throwIfExPtr();
        return write(noticeSock, PduId::PROD_INFO_NOTICE) &&
            write(noticeSock, notice);
    }
    bool notify(const DataSegId& notice) {
        LOG_TRACE;
        throwIfExPtr();
        return write(noticeSock, PduId::DATA_SEG_NOTICE) &&
            write(noticeSock, notice);
    }

    /**
     * Requests product information from the remote peer. Blocks while writing.
     *
     * @param[in] prodIndex   Index of product
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool request(const ProdIndex prodIndex) {
        throwIfExPtr();
        Guard guard{sockMutex}; // To support internal & external threads
        return write(requestSock, PduId::PROD_INFO_REQUEST) &&
                write(requestSock, prodIndex);
    }

    /**
     * Requests a data-segment from the remote peer. Blocks while writing.
     *
     * @param[in] segId       ID of data-segment
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool request(const DataSegId& segId) {
        throwIfExPtr();
        Guard guard{sockMutex}; // To support internal & external threads
        return write(requestSock, PduId::DATA_SEG_REQUEST) &&
            write(requestSock, segId);
    }

    /**
     * Sends data to the remote peer.
     *
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool send(const ProdInfo& data) {
        throwIfExPtr();
        return write(dataSock, PduId::PROD_INFO) &&
                write(dataSock, data);
    }
    bool send(const DataSeg& data) {
        throwIfExPtr();
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
    return pImpl->start();
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
            auto sock = srvrSock.accept();
            if (sock)
                Thread(&Impl::acceptSock, this, sock).detach();
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

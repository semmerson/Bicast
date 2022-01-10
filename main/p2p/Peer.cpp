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

#include "error.h"
#include "HycastProto.h"
#include "Peer.h"
#include "ThreadException.h"

#include <atomic>
#include <cassert>
#include <functional>
#include <list>
#include <sstream>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace hycast {

/**
 * Abstract base implementation of the `Peer` class.
 */
class Peer::Impl
{
    using NoticeQ = std::queue<DatumId>;

    mutable Mutex      exceptMutex;   ///< For accessing thread exception
    mutable Mutex      noticeMutex;   ///< For accessing notice queue
    mutable Cond       noticeCond;    ///< For accessing notice queue
    Thread             noticeWriter;  ///< For sending notices
    Thread             requestReader; ///< For receiving requests
    P2pMgr&            p2pMgr;        ///< Associated P2P manager
    const bool         clientSide;    ///< This instance initiated contact?
    NoticeQ            noticeQ;       ///< Notice queue
    std::exception_ptr exPtr;         ///< Internal thread exception
    const bool         iAmPub;        ///< This instance is publisher's

    /**
     * Puts sockets in increasing order according to the local port number.
     *
     * @param[in,out] socks  Sockets to be put in order
     */
    void orderClntSocks(TcpSock socks[3]) const noexcept {
        if (socks[1].getLclPort() < socks[0].getLclPort())
            socks[1].swap(socks[0]);

        if (socks[2].getLclPort() < socks[1].getLclPort()) {
            socks[2].swap(socks[1]);
            if (socks[1].getLclPort() < socks[0].getLclPort())
                socks[1].swap(socks[0]);
        }
    }

    /**
     * Puts sockets in increasing order according to the remote port number.
     *
     * @param[in,out] socks  Sockets to be put in order
     */
    void orderSrvrSocks(TcpSock socks[3]) const noexcept {
        if (socks[1].getRmtPort() < socks[0].getRmtPort())
            socks[1].swap(socks[0]);

        if (socks[2].getRmtPort() < socks[1].getRmtPort()) {
            socks[2].swap(socks[1]);
            if (socks[1].getRmtPort() < socks[0].getRmtPort())
                socks[1].swap(socks[0]);
        }
    }

    /**
     * Connects this client-side instance to its remote peer.
     *
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect
     */
    void connect() {
        TcpClntSock socks[3]; // RAII sockets => sockets close if !success

        /*
         * Connect to Peer server. Keep consonant with `PeerSrvr::accept()`.
         */

        socks[0] = TcpClntSock{rmtSockAddr};

        const in_port_t noticePort = socks[0].getLclPort();

        if (socks[0].write(noticePort)) {
            socks[1] = TcpClntSock{rmtSockAddr};

            if (socks[1].write(noticePort)) {
                socks[2] = TcpClntSock{rmtSockAddr};

                if (socks[2].write(noticePort))
                    assignClntSocks(socks);
            }
        }
    }

    /**
     * Exchanges protocol version with remote peer.
     *
     * @retval    `true`      Success
     * @retval    `false`     Connection lost
     * @throw     LogicError  Remote peer uses unsupported protocol
     */
    bool xchgProtoVers() {
        bool success = false;
        auto protoVers = PROTOCOL_VERSION;

        if (clientSide) {
            if (noticeXprt.read(protoVers)) {
                LOG_DEBUG("Peer %s received protocol version %d",
                        to_string().data(), protoVers);
                LOG_DEBUG("Peer %s is sending protocol version %d",
                        to_string().data(), PROTOCOL_VERSION);
                success = noticeXprt.write(PROTOCOL_VERSION);
            }
        }
        else {
            LOG_DEBUG("Peer %s is sending protocol version %d",
                    to_string().data(), PROTOCOL_VERSION);
            if (noticeXprt.write(PROTOCOL_VERSION) &&
                    noticeXprt.read(protoVers)) {
                LOG_DEBUG("Peer %s received protocol version %d",
                        to_string().data(), protoVers);
                success = true;
            }
        }

        if (success)
            vetProtoVers(protoVers, noticeXprt.getRmtAddr().getInetAddr());

        return success;
    }

    bool sendIsPub() {
        LOG_DEBUG("Peer %s is sending %s", to_string().data(),
                iAmPub ? "true" : "false");
        return noticeXprt.write(iAmPub);
    }

    bool recvIsPub() {
        bool success = noticeXprt.read(rmtIsPub);
        if (success)
            LOG_DEBUG("Peer %s received %s", to_string().data(),
                    rmtIsPub ? "true" : "false");
        return success;
    }

    void runNoticeWriter(Peer peer) {
        try {
            while (connected) {
                DatumId datumId;
                {
                    Lock lock{noticeMutex};
                    noticeCond.wait(lock, [&]{return !noticeQ.empty();});

                    datumId = noticeQ.front();
                    noticeQ.pop();
                }

                if (datumId.id == DatumId::Id::PROD_INDEX) {
                    LOG_DEBUG("Peer %s is notifying about product %s",
                            to_string().data(), datumId.to_string().data());
                    connected = noticeXprt.send(PduId::PROD_INFO_NOTICE,
                            datumId.prodIndex);
                }
                else if (datumId.id == DatumId::Id::DATA_SEG_ID) {
                    LOG_DEBUG("Peer %s is notifying about data-segment %s",
                            to_string().data(), datumId.to_string().data());
                    connected = noticeXprt.send(PduId::DATA_SEG_NOTICE,
                            datumId.dataSegId);
                }
                else {
                    throw LOGIC_ERROR("Datum ID is unset");
                }
            }
        }
        catch (const std::exception& ex) {
            log_error(ex);
            setExPtr();
        }
        connected = false;
        LOG_DEBUG("Terminating");
    }

    /**
     * Receives a request for data from the remote peer. Sends the data to the
     * remote peer if it exists.
     *
     * @tparam    ID       Identifier of requested data
     * @tparam    DATA     Type of requested data
     * @param[in] xprt     Transport
     * @param[in] peer     Associated local peer
     * @param[in] desc     Description of datum
     * @retval    `true`   Data doesn't exist or was successfully sent
     * @retval    `false`  Connection lost
     */
    template<class ID, class DATA>
    bool recvRequest(Xprt& xprt, Peer peer, const char* const desc) {
        bool success = false;
        ID   id;
        if (id.read(xprt)) {
            LOG_DEBUG("Peer %s received request for %s %s",
                    to_string().data(), desc, id.to_string().data());
            auto data = p2pMgr.recvRequest(id, peer);
            success = !data || send(data); // Data doesn't exist or was sent
        }
        return success;
    }

    /**
     * Dispatch function for processing requests from the remote peer.
     *
     * @param[in] pduId         PDU ID
     * @param[in] peer          Associated local peer
     * @retval    `false`       Connection lost
     * @retval    `true`        Success
     * @throw     LogicError    Message is unknown or unsupported
     */
    bool processRequest(Xprt::PduId pduId, Peer peer) {
        bool success = false;

        if (PduId::PROD_INFO_REQUEST == pduId) {
            success = recvRequest<ProdIndex, ProdInfo>(requestXprt, peer,
                    "information on product");
        }
        else if (PduId::DATA_SEG_REQUEST == pduId) {
            success = recvRequest<DataSegId, DataSeg>(requestXprt, peer,
                    "data-segment");
        }
        else {
            throw LOGIC_ERROR("Peer " + to_string() +
                    " received invalid PDU type: " + std::to_string(pduId));
        }

        return success;
    }

protected:
    /*
     * If a single socket is used for asynchronous communication and reading and
     * writing occur on the same thread, then deadlock will occur if both
     * receive buffers are full and each end is trying to write. To prevent
     * this, three sockets are used and a thread that reads from one socket will
     * write to another. The pipeline might block for a while as messages are
     * processed, but it won't deadlock.
     */
    Xprt               noticeXprt;  ///< Notice transport
    Xprt               requestXprt; ///< Request transport
    Xprt               dataXprt;    ///< Data transport
    SockAddr           rmtSockAddr; ///< Remote socket address
    SockAddr           lclSockAddr; ///< Local (notice) socket address
    std::atomic<bool>  connected;   ///< Connected to remote peer?
    bool               rmtIsPub;    ///< Remote peer is publisher's
    Mutex              startMutex;  ///< Protects `started`
    bool               started;     ///< This instance has been started?

    /**
     * Assigns sockets to this instance's notice, request, and data transports.
     *
     * @param[in] socks  Sockets to be assigned
     */
    void assignSocks(TcpSock socks[3]) {
        noticeXprt  = Xprt{socks[0]};
        requestXprt = Xprt{socks[1]};
        dataXprt    = Xprt{socks[2]};
    }

    /**
     * Assigns server-side-created sockets to this instance's notice, request,
     * and data sockets
     *
     * @param[in,out] socks  Sockets to be assigned
     */
    void assignSrvrSocks(TcpSock socks[3]) {
        orderSrvrSocks(socks);
        assignSocks(socks);
    }

    /**
     * Assigns client-side created sockets to this instance's notice, request,
     * and data sockets
     *
     * @param[in,out] socks  Sockets to be assigned
     */
    void assignClntSocks(TcpSock socks[3]) {
        orderClntSocks(socks);
        assignSocks(socks);
    }

    /**
     * Vets the protocol version used by the remote peer.
     *
     * @param[in] protoVers  Remote protocol version
     * @param[in] rmtAddr    IP address of remote peer
     * @throw LogicError     Remote peer uses unsupported protocol
     */
    void vetProtoVers(decltype(PROTOCOL_VERSION) protoVers,
                      const InetAddr             rmtAddr) {
        if (protoVers != PROTOCOL_VERSION)
            throw LOGIC_ERROR("Peer " + to_string() +
                    " received incompatible protocol version " +
                    std::to_string(protoVers) + "; not " +
                    std::to_string(PROTOCOL_VERSION));
    }

    /**
     * Sets the internal thread exception.
     */
    void setExPtr() {
        Guard guard{exceptMutex};
        if (!exPtr)
            exPtr = std::current_exception();
    }

    /**
     * Throws an exception if this instance hasn't been started or an exception
     * was thrown by one of this instance's internal threads.
     *
     * @throw LogicError  Instance hasn't been started
     * @see   `start()`
     */
    void throwIf() {
        {
            Guard guard{startMutex};
            if (!started)
                throw LOGIC_ERROR("Instance hasn't been started");
        }

        bool throwEx = false;
        {
            Guard guard{exceptMutex};
            throwEx = static_cast<bool>(exPtr);
        }
        if (throwEx)
            std::rethrow_exception(exPtr);
    }

    /**
     * Sends data to the remote peer.
     *
     * @param[in] pduId    Protocol Data Unit identifier
     * @param[in] data     Data
     * @retval    `false`  No connection. Connection was lost or `start()`
     *                     wasn't called.
     * @retval    `true`   Success
     */
    bool send(PduId pduId, const XprtAble& data) {
        if (!connected)
            return false;

        return connected = dataXprt.send(pduId, data);
    }
    inline bool send(const ProdInfo data) {
        LOG_DEBUG("Peer %s is sending product information %s",
                to_string().data(), data.to_string().data());
        return send(PduId::PROD_INFO, data);
    }
    inline bool send(const DataSeg data) {
        LOG_DEBUG("Peer %s is sending data segment %s",
                to_string().data(), data.to_string().data());
        return send(PduId::DATA_SEG, data);
    }

    /**
     * Reads and processes messages from the remote peer. Doesn't return until
     * either EOF is encountered or an error occurs.
     *
     * @param[in] xprt      Transport
     * @param[in] dispatch  Dispatch function for processing incoming PDU-s
     * @throw LogicError    Message type is unknown or unsupported
     */
    void runReader(Xprt xprt, Xprt::Dispatch dispatch) {
        try {
            while (connected) {
                connected = xprt.recv(dispatch);
            }
            LOG_NOTE("Connection %s closed", xprt.to_string().data());
        }
        catch (const std::exception& ex) {
            log_error(ex);
            setExPtr();
            connected = false;
        }
    }

    void startRequestReader(Peer peer) {
        requestReader = Thread(&Impl::runReader, this, requestXprt,
                [=](Xprt::PduId pduId, Xprt xprt) { // "&" capture => SIGSEGV
                        return processRequest(pduId, peer);});
        if (log_enabled(LogLevel::DEBUG)) {
            std::ostringstream threadId;
            threadId << requestReader.get_id();
            LOG_DEBUG("Request reader thread is %s", threadId.str().data());
        }
    }
    void stopRequestReader() {
        if (requestXprt) {
            LOG_DEBUG("Shutting down request transport");
            requestXprt.shutdown();
        }
        if (requestReader.joinable())
            requestReader.join();
    }

    void startNoticeWriter(Peer peer) {
        noticeWriter = Thread(&Impl::runNoticeWriter, this, peer);
    }
    void stopNoticeWriter() {
        if (noticeWriter.joinable()) {
            ::pthread_cancel(noticeWriter.native_handle());
            noticeWriter.join();
        }
    }

    /**
     * Starts the internal threads needed to read and service messages from the
     * remote peer.
     *
     * @param[in] peer  Associated local peer
     */
    virtual void startThreads(Peer peer) =0;

    /**
     * Stops and joins the threads that are reading and servicing messages from
     * the remote peer.
     */
    virtual void stopThreads() =0; // Idempotent

public:
    /**
     * Constructs client-side.
     *
     * @param[in] p2pMgr        P2P manager
     * @param[in] srvrAddr      Socket address of remote peer-server
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect. Bad failure.
     * @throw     RuntimeError  Couldn't connect. Might be temporary.
     */
    Impl(
            P2pMgr&        p2pMgr,
            const SockAddr srvrAddr)
        : exceptMutex()
        , noticeMutex()
        , noticeCond()
        , noticeWriter()
        , requestReader()
        , p2pMgr(p2pMgr)
        , clientSide(true)
        , noticeQ()
        , exPtr()
        , iAmPub(false)  // Can't be publisher's if client-side constructed
        , noticeXprt()
        , requestXprt()
        , dataXprt()
        , rmtSockAddr(srvrAddr)
        , lclSockAddr()
        , connected{false}
        , rmtIsPub(true) // Fewer threads created && lying is counterproductive
        , startMutex()
        , started(false)
    {
        connect();
        lclSockAddr = noticeXprt.getLclAddr();
        if (!xchgProtoVers() || !recvIsPub())
            throw RUNTIME_ERROR("Peer " + to_string() + " lost connection" +
                    to_string().data());
        connected = true;
    }

    /**
     * Constructs server-side.
     *
     * @param[in] p2pMgr      P2P manager
     * @param[in] socks       TCP sockets
     * @param[in] iAmPub      Does this instance belong to publisher?
     */
    Impl(
            P2pMgr&    p2pMgr,
            TcpSock    socks[3],
            const bool iAmPub)
        : exceptMutex()
        , noticeMutex()
        , noticeCond()
        , noticeWriter()
        , requestReader()
        , p2pMgr(p2pMgr)
        , clientSide(false)
        , noticeQ()
        , exPtr()
        , iAmPub(iAmPub)
        , noticeXprt()
        , requestXprt()
        , dataXprt()
        , rmtSockAddr()
        , lclSockAddr()
        , connected{false}
        , rmtIsPub(false)
        , startMutex()
        , started(false)
    {
        assignSrvrSocks(socks);
        rmtSockAddr = noticeXprt.getRmtAddr();
        lclSockAddr = noticeXprt.getLclAddr();
        if (!xchgProtoVers() || !sendIsPub())
            throw RUNTIME_ERROR("Peer " + to_string() + " lost connection");
        connected = true;
    }

    Impl(const Impl& impl) =delete; // Rule of three

    virtual ~Impl() noexcept
    {}

    Impl& operator=(const Impl& rhs) noexcept =delete; // Rule of three

    bool isClient() const noexcept {
        return clientSide;
    }

    bool isRmtPub() const noexcept {
        return rmtIsPub;
    }

    /**
     * Starts this instance. Creates threads on which
     *   - The sockets are read; and
     *   - The P2P manager is called.
     *
     * @param[in] peer     Associated local peer
     * @throw LogicError   Already called
     * @throw LogicError   Remote peer uses unsupported protocol
     * @throw SystemError  Thread couldn't be created
     * @see `stop()`
     */
    bool start(Peer peer) {
        Guard guard{startMutex};

        if (started)
            throw LOGIC_ERROR("start() already called");

        startThreads(peer);
        started = true; // `stop()` is now effective
    }

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() const noexcept {
        return rmtSockAddr;
    }

    /**
     * Stops this instance from serving its remote counterpart. Causes the
     * threads serving the remote peer to terminate. If called before `start()`,
     * then the remote peer will not have been served.
     *
     * Idempotent.
     *
     * @see   `start()`
     */
    void stop() {
        stopThreads();
    }

    size_t hash() {
        // Keep consistent with `operator<()`
        return rmtSockAddr.hash() ^ lclSockAddr.hash();
    }

    bool operator<(const Impl& rhs) {
        // Keep consistent with `hash()`
        return rmtSockAddr < rhs.rmtSockAddr
                ? true
                : rhs.rmtSockAddr < rmtSockAddr
                      ? false
                      : lclSockAddr < rhs.lclSockAddr;
    }

    /**
     * Returns a string representation of this instance.
     *
     * @return  String representation of this instance
     */
    String to_string() const {
        return "{lcl=" + noticeXprt.getLclAddr().to_string() + ", rmt=" +
                noticeXprt.getRmtAddr().to_string() + "}";
    }

    bool notify(const ProdIndex notice) {
        throwIf();

        if (connected && !rmtIsPub) {
            Guard guard{noticeMutex};
            noticeQ.push(DatumId{notice});
            noticeCond.notify_all();
        }

        return connected;
    }
    bool notify(const DataSegId notice) {
        throwIf();

        if (connected && !rmtIsPub) {
            Guard guard{noticeMutex};
            noticeQ.push(DatumId{notice});
            noticeCond.notify_all();
        }

        return connected;
    }
};

Peer::Peer(Impl* impl)
    : pImpl(impl)
{}

bool Peer::isClient() const noexcept {
    return pImpl->isClient();
}

bool Peer::isRmtPub() const noexcept {
    return pImpl->isRmtPub();
}

void Peer::start() {
    pImpl->start(*this);
}

SockAddr Peer::getRmtAddr() noexcept {
    return pImpl->getRmtAddr();
}

void Peer::stop() {
    pImpl->stop();
}

Peer::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

size_t Peer::hash() const noexcept {
    // Must be consistent with `operator<()`
    return pImpl->hash();
}

bool Peer::operator<(const Peer& rhs) const noexcept {
    // Must be consistent with `hash()`
    return *pImpl < *rhs.pImpl;
}

bool Peer::operator==(const Peer& rhs) const noexcept {
    return !(*this < rhs) && !(rhs < *this);
}

bool Peer::operator!=(const Peer& rhs) const noexcept {
    return (*this < rhs) || (rhs < *this);
}

String Peer::to_string() const {
    return pImpl ? pImpl->to_string() : "<unset>";
}

bool Peer::notify(const ProdIndex notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const DataSegId notice) const {
    return pImpl->notify(notice);
}

/******************************************************************************/

/**
 * Publisher's peer implementation. The peer will be server-side constructed.
 */
class PubPeerImpl final : public Peer::Impl
{
protected:
    void startThreads(Peer peer) override {
        try {
            startRequestReader(peer);
            startNoticeWriter(peer);
        }
        catch (const std::exception& ex) {
            stopThreads();
            throw;
        }
    }

    /**
     * Idempotent.
     */
    void stopThreads() override {
        stopNoticeWriter();
        stopRequestReader();
    }

public:
    /**
     * Constructs server-side.
     *
     * @param[in] node          Publisher's P2P node
     * @param[in] socks         Sockets connected to the remote peer
     * @throw     RuntimeError  Lost connection
     */
    PubPeerImpl(P2pMgr& p2pMgr, TcpSock socks[3])
        : Impl(p2pMgr, socks, true)
    {}

    ~PubPeerImpl() noexcept {
        stopThreads();
    }
};

PubPeer::PubPeer(P2pMgr& node, TcpSock socks[3])
    : Peer(new PubPeerImpl(node, socks))
{}

/******************************************************************************/

/**
 * Subscriber's peer implementation. May be server-side or client-side
 * constructed.
 */
class SubPeerImpl final : public Peer::Impl
{
    /**
     * A thread-safe, linked-map of datum requests.
     */
    class RequestQ {
        struct hashRequest
        {
            size_t operator()(const DatumId& request) const noexcept {
                return request.hash();
            }
        };

        /// The value component references the next entry in a linked list
        using Requests = std::unordered_map<DatumId, DatumId, hashRequest>;

        mutable Mutex mutex;
        mutable Cond  cond;
        Requests      requests;
        DatumId       head;
        DatumId       tail;
        bool          stopped;

        /**
         * Deletes the request at the head of the queue.
         *
         * @pre    Mutex is locked
         * @pre    Queue is not empty
         * @return The former head of the queue
         * @post   Mutex is locked
         */
        DatumId deleteHead() {
            auto oldHead = head;
            auto newHead = requests[head];
            requests.erase(head);
            head = newHead;
            if (!head)
                tail = head; // Queue is empty
            return oldHead;
        }

    public:
        class Iter : public std::iterator<std::input_iterator_tag, DatumId>
        {
            RequestQ& queue;
            DatumId   request;

        public:
            Iter(RequestQ& queue, const DatumId& request)
                : queue(queue)
                , request(request) {}
            Iter(const Iter& iter)
                : queue(iter.queue)
                , request(iter.request) {}
            bool operator!=(const Iter& rhs) const {
                return request ? (request != rhs.request) : false;
            }
            DatumId& operator*()  {return request;}
            Iter& operator++() {
                Guard guard{queue.mutex};
                request = queue.requests[request];
                return *this;
            }
        };

        /**
         * Default constructs.
         */
        RequestQ()
            : mutex()
            , cond()
            , requests()
            , head()
            , tail()
            , stopped(false)
        {}

        RequestQ(const RequestQ& queue) =delete;
        RequestQ operator=(const RequestQ& queue) =delete;
        ~RequestQ() =default;

        /**
         * Adds a request to the tail of the queue.
         *
         * @param[in] request  Request to be added
         * @return             Number of requests in the queue
         */
        size_t push(const DatumId& request) {
            Guard guard{mutex};
            requests.insert({request, DatumId{}});
            if (tail)
                requests[tail] = request;
            if (!head)
                head = request;
            tail = request;
            cond.notify_all();
            return requests.size();
        }

        size_t count(const DatumId& request) {
            Guard guard{mutex};
            return requests.count(request);
        }

        /**
         * Indicates if the queue is empty.
         *
         * @retval `true`   Queue is empty
         * @retval `false`  Queue is not empty
         */
        bool empty() const {
            Guard guard{mutex};
            return requests.empty();
        }

        /**
         * Deletes the request at the head of the queue.
         *
         * @throw OutOfRange  Queue is empty
         */
        void pop() {
            Guard guard{mutex};
            if (requests.empty())
                throw OUT_OF_RANGE("Queue is empty");
            deleteHead();
        }

        /**
         * Removes and returns the request at the head of the queue. Blocks
         * until it exists or `stop()` is called.
         *
         * @return  Head request or false one if `stop()` was called
         * @see     `stop()`
         */
        DatumId waitGet() {
            Lock lock{mutex};
            cond.wait(lock, [&]{return !requests.empty() || stopped;});
            if (stopped)
                return DatumId{};
            return deleteHead();
        }

        /**
         * Causes `waitGet()` to always return a false request.
         */
        void stop() {
            Guard guard{mutex};
            stopped = true;
            cond.notify_all();
        }

        /**
         * Tells a subscriber's P2P manager about all requests in the queue in
         * the order in which they were added, then clears the queue.
         *
         * @param[in] p2pMgr  Subscriber's P2P manager
         * @param[in] peer    Associated peer
         * @see SubP2pNode::missed(ProdIndex, Peer)
         * @see SubP2pNode::missed(DataSegId, Peer)
         */
        void drainTo(SubP2pMgr& p2pMgr, Peer peer) {
            Guard guard{mutex};
            while (head) {
                if (head.id == DatumId::Id::PROD_INDEX) {
                    p2pMgr.missed(head.prodIndex, peer);
                }
                else if (head.id == DatumId::Id::DATA_SEG_ID) {
                    p2pMgr.missed(head.dataSegId, peer);
                }
                head = requests[head];
            }
            requests.clear();
            tail = head;
        }

        Iter begin() {
            Guard guard{mutex};
            return Iter(*this, head);
        }

        Iter end() {
            Guard guard{mutex};
            return Iter{*this, DatumId{}};
        }
    };

    Thread        noticeReader;
    Thread        dataReader;
    SubP2pMgr&    subP2pMgr;     ///< Subscriber's P2P manager
    RequestQ      requested;     ///< Requests sent to remote peer

    template<class TYPE>
    void processNotice(Xprt::PduId pduId, Peer peer, const char* const desc) {
        TYPE typeId;
        connected = typeId.read(noticeXprt);
        if (connected) {
            LOG_DEBUG("Peer %s received notice about %s %s",
                    to_string().data(), desc, typeId.to_string().data());
            if (subP2pMgr.recvNotice(typeId, peer)) {
                // Subscriber wants the datum
                requested.push(DatumId{typeId});
                connected = requestXprt.send(pduId, typeId);
            }
        }
    }
    /**
     * Receives a notice about an available datum. Notifies the subscriber's P2P
     * manager. Requests the datum if indicated by the P2P manager.
     *
     * @param[in] pduId     PDU identifier: `PROD_INFO_NOTICE`, `DATA_SEG_NOTICE`
     * @param[in] peer      Associated peer
     * @retval    `false`   Connection lost
     * @retval    `true`    Success
     */
    bool processNotice(Xprt::PduId pduId, Peer peer) {
        if (PduId::DATA_SEG_NOTICE == pduId) {
            processNotice<DataSegId>(PduId::DATA_SEG_REQUEST, peer,
                    "data-segment");
        }
        else if (PduId::PROD_INFO_NOTICE == pduId) {
            processNotice<ProdIndex>(PduId::PROD_INFO_REQUEST, peer,
                    "information on product");
        }
        else {
            LOG_DEBUG("Unknown PDU ID: %d", (int)pduId);
            throw LOGIC_ERROR("Invalid PDU type: " + std::to_string(pduId));
        }

        return connected;
    }

    void startNoticeReader(Peer peer) {
        noticeReader = Thread(&SubPeerImpl::runReader, this, noticeXprt,
                [=](Xprt::PduId pduId, Xprt xprt) { // "&" capture => SIGSEGV
                        return processNotice(pduId, peer);});
    }
    void stopNoticeReader() {
        if (noticeReader.joinable()) {
            ::pthread_cancel(noticeReader.native_handle());
            noticeReader.join();
        }
    }

    /**
     * Processes data from the remote peer. Passes the data to the subscriber's
     * P2P node. Removes the associated request from the requested queue.
     * If the data wasn't requested, then it is ignored and each request in the
     * requested queue is removed from the queue and the P2P node is notified
     * that it cannot be satisfied by the remote peer (NB: the requested queue
     * is emptied).
     *
     * @tparam    DATUM       Type of datum (`ProdInfo`, `DataSeg`)
     * @param[in] datum       Datum
     * @param[in] desc        Description of datum
     * @param[in] peer        Associated peer
     * @throw     LogicError  Datum wasn't requested
     * @see `Request::missed()`
     */
    template<class DATUM>
    void processData(DATUM& datum, const char* const desc, Peer peer) {
        const auto& id = datum.getId();
        if (requested.count(DatumId{id}) == 0)
            throw LOGIC_ERROR("Peer " + peer.to_string() + " received "
                    "unrequested " + desc + " " + datum.to_string());

        for (auto iter =  requested.begin(), end = requested.end();
                iter != end; ) {
            if ((*iter).equals(id)) {
                subP2pMgr.recvData(datum, peer);
                requested.pop();
                break;
            }
            /*
             *  NB: A missed response from the remote peer means that it doesn't
             *  have the requested data.
             */
            if ((*iter).id == DatumId::Id::PROD_INDEX) {
                subP2pMgr.missed((*iter).prodIndex, peer);
            }
            else if ((*iter).id == DatumId::Id::DATA_SEG_ID) {
                subP2pMgr.missed((*iter).dataSegId, peer);
            }

            ++iter; // Must occur before `requested.pop()`
            requested.pop();
        }
    }
    /**
     * Processes incoming data from remote peer.
     *
     * @param[in] pduId    Protocol data unit identifier
     * @param[in] peer     Associated local peer
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool processData(Xprt::PduId pduId, Peer peer) {
        bool success = false;

        if (PduId::DATA_SEG == pduId) {
            DataSeg dataSeg{};
            success = dataSeg.read(dataXprt);
            if (success) {
                LOG_DEBUG("Peer %s received data segment %s",
                        to_string().data(), dataSeg.to_string().data());
                processData<DataSeg>(dataSeg, "information on product", peer);
            }
        }
        else if (PduId::PROD_INFO == pduId) {
            ProdInfo prodInfo{};
            success = prodInfo.read(dataXprt);
            if (success) {
                LOG_DEBUG("Peer %s received product information %s",
                        to_string().data(), prodInfo.to_string().data());
                processData<ProdInfo>(prodInfo, "data-segment", peer);
            }
        }
        else if (PduId::PEER_SRVR_ADDRS == pduId) {
            Tracker tracker{};
            success = tracker.read(dataXprt);
            if (success) {
                LOG_DEBUG("Peer %s received peer-server addresses %s",
                        to_string().data(), tracker.to_string().data());
                subP2pMgr.recvData(tracker, peer);
            }
        }
        else {
            LOG_DEBUG("Unknown PDU ID: %d", (int)pduId);
            throw LOGIC_ERROR("Invalid PDU type: " + std::to_string(pduId));
        }

        return success;
    }

    void startDataReader(Peer peer) {
        dataReader = Thread(&SubPeerImpl::runReader, this, dataXprt,
            [=](Xprt::PduId pduId, Xprt xprt) {
                    return processData(pduId, peer);});
    }
    void stopDataReader() {
        if (dataReader.joinable()) {
            ::pthread_cancel(dataReader.native_handle());
            dataReader.join();
        }
    }

protected:
    void startThreads(Peer peer) override {
        try {
            startDataReader(peer);
            if (!rmtIsPub) // Publisher's don't make requests
                startRequestReader(peer);
            startNoticeReader(peer);
            startNoticeWriter(peer);
        }
        catch (const std::exception& ex) {
            stopThreads();
            throw;
        }
    }

    /**
     * Idempotent.
     */
    void stopThreads() override {
        if (dataXprt)
            dataXprt.shutdown();
        if (requestXprt)
            requestXprt.shutdown();
        if (noticeXprt)
            noticeXprt.shutdown();

        stopNoticeWriter();
        stopNoticeReader();
        if (!rmtIsPub) // Publisher's don't make requests
            stopRequestReader();
        stopDataReader();
    }

public:
    /**
     * Client-side construction.
     *
     * @param[in] node          Subscriber's P2P manager
     * @param[in] srvrAddr      Socket address of remote peer-server
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect. Bad failure.
     * @throw     RuntimeError  Couldn't connect. Might be temporary.
     */
    SubPeerImpl(
            SubP2pMgr&     p2pMgr,
            const SockAddr srvrAddr)
        : Impl(p2pMgr, srvrAddr)
        , subP2pMgr(p2pMgr)
        , requested()
    {}

    /**
     * Server-side construction.
     *
     * @param[in] p2pMgr  Subscriber's P2P manager
     * @param[in] socks   Sockets connected to the remote peer
     */
    SubPeerImpl(SubP2pMgr& p2pMgr, TcpSock socks[3])
        : Impl(p2pMgr, socks, false)
        , subP2pMgr(p2pMgr)
        , requested()
    {}

    ~SubPeerImpl() noexcept {
        stopThreads();
    }
};

SubPeer::SubPeer(SubP2pMgr& node, TcpSock socks[3])
    : Peer(new SubPeerImpl(node, socks))
{}

SubPeer::SubPeer(SubP2pMgr&     node,
                 const SockAddr& srvrAddr)
    : Peer(new SubPeerImpl(node, srvrAddr))
{}

/******************************************************************************/

/**
 * Peer-server implementation. Listens for connections from remote, subscribing,
 * client-side peers and creates a corresponding server-side peer.
 *
 * @tparam MGR   Type of P2P manager: `P2pMgr` or `SubP2pNode`
 * @tparam PEER  Type of peer: `PubPeer` or `SubPeer`
 */
template<class MGR, class PEER>
class PeerSrvr<MGR, PEER>::Impl
{
    /**
     * Queue of accepted peers.
     */
    using PeerQ = std::queue<PEER, std::list<PEER>>;

    /**
     * Factory for creating peers by accepting connections.
     */
    class PeerFactory
    {
        using Map = std::unordered_map<SockAddr, TcpSock[3]>;

        MGR&  mgr;
        Map   connections;

    public:
        /**
         * Constructs.
         *
         * @tparam    MGR   Type of P2P manager (publisher or subscriber)
         * @param[in] node  P2P node
         */
        PeerFactory(MGR& mgr)
            : mgr(mgr)
            , connections()
        {}

        /**
         * Adds an individual socket to a server-side peer. If the addition
         * completes the entry, then it removed from this instance.
         *
         * @param[in]  sock        Individual socket
         * @param[in]  noticePort  Port number of the notification socket
         * @param[out] peer        Associated peer. Set iff `true` is returned.
         * @retval     `false`     Peer is not complete. `peer` is not set.
         * @retval     `true`      Peer is complete. `peer` is set.
         * @threadsafety           Unsafe
         */
        bool add(TcpSock& sock, in_port_t noticePort, PEER& peer) {
            // TODO: Limit number of outstanding connections
            // TODO: Purge old entries
            auto key = sock.getRmtAddr().clone(noticePort);
            auto socks = connections[key];

            for (int i = 0; i < 3; ++i) {
                if (!socks[i]) {
                    socks[i] = sock;
                    break;
                }
            }

            if (!socks[2])
                return false;

            peer = PEER(mgr, socks);
            connections.erase(key);
            return true;
        }
    };

    mutable Mutex mutex;
    mutable Cond  cond;
    PeerFactory   peerFactory;
    TcpSrvrSock   srvrSock;         ///< Socket on which this instance listens
    PeerQ         acceptQ;          ///< Queue of accepted peers
    size_t        maxAccept;        ///< Maximum size of the peer queue

    /**
     * Executes on a separate thread.
     *
     * @param[in] sock  Newly-accepted socket
     */
    void processSock(TcpSock sock) {
        in_port_t noticePort;

        if (sock.read(noticePort)) { // Might take a while
            // The rest is fast
            Guard guard{mutex};

            if (acceptQ.size() < maxAccept) {
                PEER peer{};
                if (peerFactory.add(sock, noticePort, peer))
                    acceptQ.push(peer);
                cond.notify_one();
            }
        } // Read notice port number
    }

public:
    /**
     * Constructs from the local address for the peer-server.
     *
     * @tparam    MGR              Type of P2P manager (publisher or subscriber)
     * @param[in] mgr              P2P manager
     * @param[in] srvrAddr         Local Address for peer server
     * @param[in] maxAccept        Maximum number of outstanding peer
     *                             connections
     */
    Impl(   MGR&           mgr,
            const SockAddr srvrAddr,
            const unsigned maxAccept)
        : mutex()
        , cond()
        , peerFactory(mgr)
        , srvrSock(srvrAddr, 3*maxAccept)
        , acceptQ()
        , maxAccept(maxAccept)
    {}

    ~Impl() noexcept {
    }

    SockAddr getSockAddr() const {
        return srvrSock.getLclAddr();
    }

    /**
     * Returns the next local peer.
     *
     * @return              Next local peer.
     * @throws SystemError  `::accept()` failure
     */
    PEER accept() {
        Lock lock{mutex};

        while (acceptQ.empty()) {
            // TODO: Limit number of threads
            // TODO: Lower priority of thread to favor data transmission
            auto sock = srvrSock.accept();
            if (!sock)
                throw SYSTEM_ERROR("accept() failure");

            Thread(&Impl::processSock, this, sock).detach();
            cond.wait(lock);
        }

        auto peer = acceptQ.front();
        acceptQ.pop();

        return peer;
    }
};

template<>
PeerSrvr<P2pMgr,PubPeer>::PeerSrvr(P2pMgr&        p2pMgr,
                                   const SockAddr srvrAddr,
                                   unsigned       maxAccept)
    : pImpl()
{
    if (srvrAddr.getInetAddr().isAny())
        throw INVALID_ARGUMENT("Peer-server's address can't be wildcard");
    pImpl.reset(new Impl(p2pMgr, srvrAddr, maxAccept));
}

template<>
SockAddr PeerSrvr<P2pMgr,PubPeer>::getSockAddr() const {
    return pImpl->getSockAddr();
}

template<>
PubPeer PeerSrvr<P2pMgr,PubPeer>::accept() {
    return pImpl->accept();
}

template<>
PeerSrvr<SubP2pMgr,SubPeer>::PeerSrvr(SubP2pMgr&     p2pMgr,
                                      const SockAddr srvrAddr,
                                      unsigned       maxAccept)
    : pImpl(new Impl(p2pMgr, srvrAddr, maxAccept))
{}

template<>
SockAddr PeerSrvr<SubP2pMgr,SubPeer>::getSockAddr() const {
    return pImpl->getSockAddr();
}

template<>
SubPeer PeerSrvr<SubP2pMgr,SubPeer>::accept() {
    return pImpl->accept();
}

} // namespace

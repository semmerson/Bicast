/**
 * This file implements the Peer class. The Peer class handles low-level, bidirectional messaging
 * with its remote counterpart.
 *
 *  @file:  Peer.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */
#include "config.h"

#include "Peer.h"
#include "ThreadException.h"

#include <atomic>
#include <queue>
#include <unordered_map>

namespace hycast {

/**
 * Abstract base implementation of the `Peer` class.
 */
class PeerImpl : public Peer
{
    using NoticeQ = std::queue<Notice>;

    mutable Mutex      noticeMutex;   ///< For accessing notice queue
    mutable Cond       noticeCond;    ///< For accessing notice queue
    Thread             noticeWriter;  ///< For sending notices
    Thread             backlogThread; ///< Thread for sending backlog notices
    std::once_flag     backlogFlag;   ///< Ensures that a backlog is sent only once
    P2pMgr&            p2pMgr;        ///< Associated P2P manager
    NoticeQ            noticeQ;       ///< Notice queue
    ThreadEx           threadEx;      ///< Internal thread exception
    SockAddr           lclSockAddr;   ///< Local (notice) socket address
    bool               rmtIsPub;      ///< Remote peer is publisher's
    std::atomic<bool>  rmtIsPubPath;  ///< Remote node has a path to the publisher?

    /**
     * Handles the backlog of products missing from the remote peer.
     *
     * @pre                The current thread is detached
     * @param[in] prodIds  Identifiers of complete products that the remote per has.
     */
    void runBacklog(ProdIdSet::Pimpl prodIds) {
        LOG_ASSERT(std::this_thread::get_id() == std::thread::id());

        try {
            const auto missing = p2pMgr.subtract(prodIds); // Products missing from remote peer
            const auto maxSegSize = DataSeg::getMaxSegSize();

            // The regular P2P mechanism is used to notify the remote peer of data that it's missing
            for (const auto prodId : *missing) {
                notify(prodId);

                /*
                 * Getting data-product information will count against this instance iff the local
                 * P2P manager is the publisher's. This might cause this instance to be judged the
                 * worst peer and, consequently, disconnected. If the local P2P manager is a
                 * subscriber, however, then this won't have that effect. This is a good thing
                 * because it's better for the network if any backlogs are obtained from subscribers
                 * rather than the publisher, IMO.
                 */
                const auto prodInfo = p2pMgr.getDatum(prodId, rmtSockAddr);
                if (prodInfo) {
                    const auto prodSize = prodInfo.getSize();
                    for (SegSize offset = 0; offset < prodSize; offset += maxSegSize)
                        notify(DataSegId{prodId, offset});
                }
            }
        }
        catch (const std::exception& ex) {
            log_error(ex);
            threadEx.set(ex);
        }
    }

protected:
    Rpc::Pimpl         rpc;         ///< Remote procedure call module
    SockAddr           rmtSockAddr; ///< Remote socket address
    std::atomic<bool>  connected;   ///< Connected to remote peer?


    void runNoticeWriter() {
        LOG_DEBUG("Executing notice writer");
        try {
            while (connected) {
                Notice notice;
                {
                    Lock lock{noticeMutex};
                    noticeCond.wait(lock, [&]{return !noticeQ.empty();});

                    notice = noticeQ.front();
                    noticeQ.pop();
                }

                if (notice.id == Notice::Id::DATA_SEG_ID) {
                    LOG_DEBUG("Peer %s is notifying about data-segment %s",
                            to_string().data(), notice.to_string().data());
                    connected = rpc->notify(notice.dataSegId);
                }
                else if (notice.id == Notice::Id::PROD_INDEX) {
                    LOG_DEBUG("Peer %s is notifying about product %s",
                            to_string().data(), notice.to_string().data());
                    connected = rpc->notify(notice.prodId);
                }
                else if (notice.id == Notice::Id::AM_PUB_PATH) {
                    LOG_DEBUG("Peer %s is notifying that it %s a path to the publisher",
                            to_string().data(), notice.amPubPath ? "is" : "is not");
                    connected = rpc->notifyAmPubPath(notice.amPubPath);
                }
                else if (notice.id == Notice::Id::GOOD_P2P_SRVR) {
                    LOG_DEBUG("Peer %s is notifying about good P2P server %s",
                            to_string().data(), notice.to_string().data());
                    connected = rpc->add(notice.srvrAddr);
                }
                else if (notice.id == Notice::Id::GOOD_P2P_SRVRS) {
                    LOG_DEBUG("Peer %s is notifying about good P2P servers %s",
                            to_string().data(), notice.to_string().data());
                    connected = rpc->add(notice.tracker);
                }
                else if (notice.id == Notice::Id::BAD_P2P_SRVR) {
                    LOG_DEBUG("Peer %s is notifying about bad P2P server %s",
                            to_string().data(), notice.to_string().data());
                    connected = rpc->remove(notice.srvrAddr);
                }
                else if (notice.id == Notice::Id::BAD_P2P_SRVRS) {
                    LOG_DEBUG("Peer %s is notifying about bad P2P servers %s",
                            to_string().data(), notice.to_string().data());
                    connected = rpc->remove(notice.tracker);
                }
                else {
                    throw LOGIC_ERROR("Notice ID is unknown: " + std::to_string((int)notice.id));
                }
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
            threadEx.set(ex);
        }
        connected = false;
        LOG_DEBUG("Terminating");
    }

    void startNoticeWriter() {
        //LOG_DEBUG("Starting notice-writer thread");
        noticeWriter = Thread(&PeerImpl::runNoticeWriter, this);
    }
    void stopNoticeWriter() {
        if (noticeWriter.joinable()) {
            ::pthread_cancel(noticeWriter.native_handle());
            noticeWriter.join();
        }
    }


    bool addNotice(const Notice& notice) {
        threadEx.throwIfSet();

        if (connected && !rmtIsPub) {
            //LOG_DEBUG("Peer %s is adding notice %s", to_string().data(),
                    //notice.to_string().data());
            Guard guard{noticeMutex};
            noticeQ.push(notice);
            noticeCond.notify_all();
        }

        return connected;
    }

    /**
     * Stops this instance from serving its remote counterpart. Causes the threads serving the
     * remote peer to terminate.
     *
     * Idempotent.
     */
    void stopImpl() {
        stopNoticeWriter();
        rpc->stop();
    }

public:
    /**
     * Constructs.
     *
     * @param[in] p2pMgr          P2P manager
     * @param[in] rpc             Pointer to RPC module
     * @param[in] rmtIsPathToPub  Remote counterpart is a path to the publisher?
     */
    PeerImpl(
            P2pMgr&    p2pMgr,
            Rpc::Pimpl rpc,
            const bool rmtIsPathToPub)
        : noticeMutex()
        , noticeCond()
        , noticeWriter()
        , backlogThread()
        , backlogFlag()
        , p2pMgr(p2pMgr)
        , noticeQ()
        , threadEx()
        , lclSockAddr(rpc->getLclAddr())
        , rmtIsPub(rpc->isRmtPub())
        , rmtIsPubPath(rmtIsPathToPub)
        , rpc(rpc)
        , rmtSockAddr(rpc->getRmtAddr())
        , connected{true}
    {}

    PeerImpl(const PeerImpl& impl) =delete; // Rule of three

    virtual ~PeerImpl() noexcept {}

    PeerImpl& operator=(const PeerImpl& rhs) noexcept =delete; // Rule of three

    /**
     * Returns the socket address of the local peer.
     *
     * @return Socket address of local peer
     */
    SockAddr getLclAddr() const noexcept override {
        return lclSockAddr;
    }

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() const noexcept override {
        return rmtSockAddr;
    }

    size_t hash() const noexcept override {
        // Keep consistent with `operator<()`
        return rmtSockAddr.hash() ^ lclSockAddr.hash();
    }

    bool operator<(const Peer& rhs) const noexcept override {
        // Keep consistent with `hash()`
        return rmtSockAddr < rhs.getRmtAddr()
                ? true
                : rhs.getRmtAddr() < rmtSockAddr
                      ? false
                      : lclSockAddr < rhs.getLclAddr();
    }

    bool operator!=(const Peer& rhs) const noexcept override {
        return *this < rhs || rhs < *this;
    }

    bool operator==(const Peer& rhs) const noexcept override {
        return !(*this == rhs);
    }

    /**
     * Returns a string representation of this instance.
     *
     * @return  String representation of this instance
     */
    String to_string() const override {
        return "{lcl=" + lclSockAddr.to_string() + ", rmt=" + rmtSockAddr.to_string() + "}";
    }

    void start() override {
        startNoticeWriter();
        rpc->start(*this); // Services incoming calls
    }

    /**
     * Stops this instance from serving its remote counterpart. Causes the threads serving the
     * remote peer to terminate.
     *
     * Idempotent.
     *
     * @see   `start()`
     */
    void stop() override {
        stopImpl();
    }

    bool notifyHavePubPath(const bool havePubPath) override {
        return addNotice(Notice{havePubPath});
    }

    bool add(const SockAddr p2pSrvr) override {
        return addNotice(Notice{p2pSrvr});
    }
    bool add(const Tracker  tracker) override {
        return addNotice(Notice{tracker});
    }

    bool remove(const SockAddr p2pSrvr) override {
        return addNotice(Notice{p2pSrvr, false});
    }
    bool remove(const Tracker tracker) override {
        return addNotice(Notice{tracker, false});
    }

    void recvHavePubPath(const bool havePubPath) override {
        rmtIsPubPath = havePubPath;
    }

    bool isRmtPathToPub() const noexcept override {
        return rmtIsPubPath;
    }

    bool notify(const ProdId prodId) override {
        return addNotice(Notice{prodId});
    }
    bool notify(const DataSegId segId) override {
        return addNotice(Notice{segId});
    }

    void recvHaveProds(ProdIdSet::Pimpl prodIds) override {
        if (isRmtPub())
            throw LOGIC_ERROR("Remote peer is publisher yet sent a backlog request");

        std:call_once(backlogFlag, [&]{
                backlogThread = Thread(&PeerImpl::runBacklog, this, prodIds);
                backlogThread.detach();});
    }

    /**
     * Processes a request for product information.
     *
     * @param[in] prodId     Index of the product
     */
    bool recvRequest(const ProdId prodId) override {
        LOG_DEBUG("Peer %s received request for information on product %s", to_string().data(),
                prodId.to_string().data());
        auto prodInfo = p2pMgr.getDatum(prodId, rmtSockAddr);
        if (prodInfo) {
            LOG_DEBUG("Peer %s is sending information on product %s", to_string().data(),
                    prodId.to_string().data());
            connected = rpc->send(prodInfo);
        }
        return connected;
    }

    /**
     * Processes a request for a data segment.
     *
     * @param[in] dataSegId     Data segment ID
     */
    bool recvRequest(const DataSegId dataSegId) override {
        LOG_DEBUG("Peer %s received request for data segment %s", to_string().data(),
                dataSegId.to_string().data());
        auto dataSeg = p2pMgr.getDatum(dataSegId, rmtSockAddr);
        if (dataSeg)
            connected = rpc->send(dataSeg);
        return connected;
    }
};

/******************************************************************************/

/**
 * Publisher's peer implementation. The peer will be constructed server-side.
 */
class PubPeerImpl final : public PeerImpl
{
public:
    /**
     * Constructs.
     *
     * @param[in] p2pMgr        Publisher's P2P manager
     * @param[in] rpc           RPC module
     * @throw     RuntimeError  Lost connection
     */
    PubPeerImpl(
            PubP2pMgr& p2pMgr,
            Rpc::Pimpl rpc)
        : PeerImpl(p2pMgr, rpc, true) // Remote peer is, obviously, a path to this end
    {}

    ~PubPeerImpl() noexcept {
        try {
            stopImpl(); // Idempotent
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    bool isPub() const noexcept override {
        return true;
    }

    bool isRmtPub() const noexcept override {
        return false;
    }

    bool isClient() const noexcept override {
        return false;
    }

    void recvAdd(const SockAddr p2pSrvr) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void recvAdd(const Tracker tracker) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }

    void recvRemove(const SockAddr p2pSrvr) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void recvRemove(const Tracker tracker) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }

    bool recvNotice(const ProdId prodId) override {
        throw LOGIC_ERROR("Shouldn't have been called");
        return false;
    }
    bool recvNotice(const DataSegId dataSegId) override {
        throw LOGIC_ERROR("Shouldn't have been called");
        return false;
    }

    void recvData(const ProdInfo prodInfo) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void recvData(const DataSeg dataSeg) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
};

Peer::Pimpl Peer::create(
        PubP2pMgr& p2pMgr,
        Rpc::Pimpl rpc) {
    return Pimpl(new PubPeerImpl(p2pMgr, rpc));
}

/**************************************************************************************************/

/**
 * Subscriber's peer implementation. May be constructed server-side or client-side.
 */
class SubPeerImpl final : public PeerImpl
{
    /**
     * A thread-safe, linked-map of datum requests.
     */
    class RequestQ {
        struct hashRequest
        {
            size_t operator()(const Notice& request) const noexcept {
                return request.hash();
            }
        };

        /// The value component references the next entry in a linked list
        using Requests = std::unordered_map<Notice, Notice, hashRequest>;

        mutable Mutex mutex;
        mutable Cond  cond;
        Requests      requests;
        Notice        head;
        Notice        tail;
        bool          stopped;

        /**
         * Deletes the request at the head of the queue.
         *
         * @pre    Mutex is locked
         * @pre    Queue is not empty
         * @return The former head of the queue
         * @post   Mutex is locked
         */
        Notice deleteHead() {
            auto oldHead = head;
            auto newHead = requests[head];
            requests.erase(head);
            head = newHead;
            if (!head)
                tail = head; // Queue is empty
            return oldHead;
        }

    public:
        class Iter : public std::iterator<std::input_iterator_tag, Notice>
        {
            RequestQ& queue;
            Notice   request;

        public:
            Iter(RequestQ& queue, const Notice& request)
                : queue(queue)
                , request(request) {}
            Iter(const Iter& iter)
                : queue(iter.queue)
                , request(iter.request) {}
            bool operator!=(const Iter& rhs) const {
                return request ? (request != rhs.request) : false;
            }
            Notice& operator*()  {return request;}
            Notice* operator->() {return &request;}
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
        size_t push(const Notice& request) {
            Guard guard{mutex};
            requests.insert({request, Notice{}});
            if (tail)
                requests[tail] = request;
            if (!head)
                head = request;
            tail = request;
            cond.notify_all();
            return requests.size();
        }

        size_t count(const Notice& request) {
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
         * Removes and returns the request at the head of the queue. Blocks until it exists or
         * `stop()` is called.
         *
         * @return  Head request or false one if `stop()` was called
         * @see     `stop()`
         */
        Notice waitGet() {
            Lock lock{mutex};
            cond.wait(lock, [&]{return !requests.empty() || stopped;});
            if (stopped)
                return Notice{};
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
         * Tells a subscriber's P2P manager about all requests in the queue in the order in which
         * they were added, then clears the queue.
         *
         * @param[in] p2pMgr       Subscriber's P2P manager
         * @param[in] rmtSockAddr  Socket address of the remote peer
         * @see SubP2pNode::missed(ProdId, SockAddr)
         * @see SubP2pNode::missed(DataSegId, SockAddr)
         */
        void drainTo(
                SubP2pMgr&      p2pMgr,
                const SockAddr rmtSockAddr) {
            Guard guard{mutex};
            while (head) {
                if (head.id == Notice::Id::PROD_INDEX) {
                    p2pMgr.missed(head.prodId, rmtSockAddr);
                }
                else if (head.id == Notice::Id::DATA_SEG_ID) {
                    p2pMgr.missed(head.dataSegId, rmtSockAddr);
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
            return Iter{*this, Notice{}};
        }
    };

    SubP2pMgr&     subP2pMgr;   ///< Subscriber's P2P manager
    RequestQ       requested;   ///< Requests sent to remote peer
    std::once_flag backlogFlag; ///< Ensures that the backlog is requested only once

    /**
     * Requests the backlog of data that's already been transmitted but that this instance doesn't
     * have.
     */
    void requestBacklog() {
        const auto prodIds = subP2pMgr.getProdIds(); // All complete products
        if (prodIds)
            /**
             * Informing the remote peer of data this instance doesn't know about is impossible;
             * therefore, the remote peer is informed of data this instance has, which will cause it
             * to notify this instance of any missing data.
             */
            connected = rpc->send(prodIds);
    }

    /**
     * Received a datum from the remote peer. If the datum wasn't requested, then it is ignored and
     * each request in the requested queue before the request for the given datum is removed from
     * the queue and passed to the P2P manager as not satisfiable by the remote peer. Otherwise, the
     * datum is passed to the subscriber's P2P manager and the pending request is removed from the
     * requested queue.
     *
     * @tparam    DATUM       Type of datum: `ProdInfo` or `DataSeg`
     * @param[in] datum       Datum
     * @throw     LogicError  Datum wasn't requested
     * @see `SubP2pMgr::missed()`
     */
    template<typename DATUM>
    void processData(const DATUM datum) {
        const auto& id = datum.getId();
        if (requested.count(Notice{id}) == 0)
            throw LOGIC_ERROR("Peer " + to_string() + " received unrequested datum " +
                    datum.to_string());

        for (auto iter =  requested.begin(), end = requested.end(); iter != end; ) {
            if (iter->equals(id)) {
                subP2pMgr.recvData(datum, rmtSockAddr);
                requested.pop();
                return;
            }
            /*
             *  NB: A missed response from the remote peer means that it doesn't have the requested
             *  data.
             */
            if (iter->getType() == Notice::Id::DATA_SEG_ID) {
                subP2pMgr.missed(iter->dataSegId, rmtSockAddr);
            }
            else if (iter->getType() == Notice::Id::PROD_INDEX) {
                subP2pMgr.missed(iter->prodId, rmtSockAddr);
            }

            ++iter; // Must occur before `requested.pop()`
            requested.pop();
        }
        LOG_DEBUG("Datum " + datum.to_string() + " wasn't requested");
    }

public:
    /**
     * Constructs -- both client- and server-side.
     *
     * @param[in] p2pMgr        Subscriber's P2P manager
     * @param[in] rpc           Pointer to RPC module
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect. Bad failure.
     * @throw     RuntimeError  Couldn't connect. Might be temporary.
     */
    SubPeerImpl(
            SubP2pMgr& p2pMgr,
            Rpc::Pimpl rpc)
        : PeerImpl(p2pMgr, rpc, rpc->isRmtPub())
        , subP2pMgr(p2pMgr)
        , requested()
        , backlogFlag()
    {
        // TODO: Enable
        // if (!rpc->sendSrvrAddr(subP2pMgr.getSrvrAddr()))
            // throw RUNTIME_ERROR("rpc::sendSrvrAddr() failure");
    }

    ~SubPeerImpl() noexcept {
        try {
            stopImpl();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    bool isClient() const noexcept override {
        return rpc->isClient();
    }

    bool isPub() const noexcept override {
        return false;
    }

    bool isRmtPub() const noexcept override {
        return rpc->isRmtPub();
    }

    void recvAdd(const SockAddr p2pSrvr) override {
        subP2pMgr.recvAdd(p2pSrvr);
    }
    void recvAdd(const Tracker tracker) override {
        subP2pMgr.recvAdd(tracker);
    }

    void recvRemove(const SockAddr p2pSrvr) override {
        subP2pMgr.recvRemove(p2pSrvr);
    }
    void recvRemove(const Tracker tracker) override {
        subP2pMgr.recvRemove(tracker);
    }

    /**
     * Receives a notice about available product information. Notifies the subscriber's P2P manager.
     * Requests the datum if told to do so by the P2P manager.
     *
     * The first time this function is called, it tells the remote peer what complete data-products
     * it has.
     *
     * @param[in] prodId     Product index
     * @retval    `false`    Connection lost
     * @retval    `true`     Success
     */
    bool recvNotice(const ProdId prodId) {
        LOG_DEBUG("Peer %s received notice about information on product %s",
                to_string().data(), prodId.to_string().data());

        std::call_once(backlogFlag, [&]{requestBacklog();});

        if (subP2pMgr.recvNotice(prodId, rmtSockAddr)) {
            // Subscriber wants the product information
            requested.push(Notice{prodId});
            LOG_DEBUG("Peer %s is requesting information on product %s", to_string().data(),
                    prodId.to_string().data());
            connected = rpc->request(prodId);
        }
        return connected;
    }
    /**
     * Receives a notice about an available data segment. Notifies the subscriber's P2P manager.
     * Requests the datum if told to do so by the P2P manager.
     *
     * The first time this function is called, it tells the remote peer what complete data-products
     * it has.
     *
     * @param[in] dataSegId  Data segment ID
     * @retval    `false`    Connection lost
     * @retval    `true`     Success
     */
    bool recvNotice(const DataSegId dataSegId) {
        LOG_DEBUG("Peer %s received notice about data segment %s", to_string().data(),
                dataSegId.to_string().data());

        std::call_once(backlogFlag, [&]{requestBacklog();});

        if (subP2pMgr.recvNotice(dataSegId, rmtSockAddr)) {
            // Subscriber wants the data segment
            requested.push(Notice{dataSegId});
            LOG_DEBUG("Peer %s is requesting data segment %s", to_string().data(),
                    dataSegId.to_string().data());
            connected = rpc->request(dataSegId);
        }
        return connected;
    }

    /**
     * Receives product information from the remote peer.
     *
     * @param[in] prodInfo  Product information
     */
    void recvData(const ProdInfo prodInfo) {
        LOG_DEBUG("Peer %s received information on product %s",
                to_string().data(), prodInfo.getId().to_string().data());
        processData<ProdInfo>(prodInfo);
    }
    /**
     * Receives a data segment from the remote peer.
     *
     * @param[in] dataSeg  Data segment
     */
    void recvData(const DataSeg dataSeg) {
        LOG_DEBUG("Peer %s received data-segment %s", to_string().data(),
                dataSeg.to_string().data());
        processData<DataSeg>(dataSeg);
    }
};

Peer::Pimpl Peer::create(
        SubP2pMgr& p2pMgr,
        Rpc::Pimpl rpc) {
    return Pimpl(new SubPeerImpl(p2pMgr, rpc));
}

Peer::Pimpl Peer::create(
        SubP2pMgr&     p2pMgr,
        const SockAddr srvrAddr) {
    return create(p2pMgr, Rpc::create(srvrAddr));
}

/******************************************************************************/

template<typename P2P_MGR>
class PeerSrvrImpl : public PeerSrvr<P2P_MGR>
{
    typename RpcSrvr::Pimpl rpcSrvr;

public:
    PeerSrvrImpl(
            const TcpSrvrSock p2pSrvr,
            const bool        iAmPub,
            const unsigned    acceptQSize)
        : rpcSrvr(RpcSrvr::create(p2pSrvr, iAmPub, acceptQSize))
    {}

    PeerSrvrImpl(
            const SockAddr srvrAddr,
            const bool     iAmPub,
            const unsigned acceptQSize)
        : rpcSrvr(RpcSrvr::create(srvrAddr, iAmPub, acceptQSize))
    {}

    SockAddr getSrvrAddr() const override {
        return rpcSrvr->getSrvrAddr();
    }

    Peer::Pimpl accept(P2P_MGR& p2pMgr) override {
        auto rpc = rpcSrvr->accept();
        return Peer::create(p2pMgr, rpc);
    }
};

template<>
PeerSrvr<PubP2pMgr>::Pimpl PeerSrvr<PubP2pMgr>::create(
        const TcpSrvrSock srvrSock,
        const unsigned    acceptQSize) {
    return Pimpl{new PeerSrvrImpl<PubP2pMgr>(srvrSock, true, acceptQSize)};
}

template<>
PeerSrvr<PubP2pMgr>::Pimpl PeerSrvr<PubP2pMgr>::create(
        const SockAddr  srvrAddr,
        const unsigned  acceptQSize) {
    auto srvrSock = TcpSrvrSock(srvrAddr, 3*acceptQSize); // Connection comprises 3 separate sockets
    return create(srvrSock, acceptQSize);
}

template<>
PeerSrvr<SubP2pMgr>::Pimpl PeerSrvr<SubP2pMgr>::create(
        const TcpSrvrSock srvrSock,
        const unsigned    acceptQSize) {
    return Pimpl{new PeerSrvrImpl<SubP2pMgr>(srvrSock, false, acceptQSize)};
}

template<>
PeerSrvr<SubP2pMgr>::Pimpl PeerSrvr<SubP2pMgr>::create(
        const SockAddr srvrAddr,
        const unsigned acceptQSize) {
    return Pimpl{new PeerSrvrImpl<SubP2pMgr>(srvrAddr, false, acceptQSize)};
}

} // namespace

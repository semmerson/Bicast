/**
 * This file implements the Peer class. The Peer class handles low-level,
 * bidirectional messaging with its remote counterpart.
 *
 *  @file:  Peer.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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
class PeerImpl : public Peer
{
    using NoticeQ = std::queue<DatumId>;

protected:
    mutable Mutex      exceptMutex;   ///< For accessing thread exception
    mutable Mutex      noticeMutex;   ///< For accessing notice queue
    mutable Cond       noticeCond;    ///< For accessing notice queue
    Thread             noticeWriter;  ///< For sending notices
    P2pMgr&            p2pMgr;        ///< Associated P2P manager
    Rpc::Pimpl         rpc;           ///< Remote procedure call module
    NoticeQ            noticeQ;       ///< Notice queue
    std::exception_ptr exPtr;         ///< Internal thread exception

    void runNoticeWriter() {
        LOG_DEBUG("Executing notice writer");
        try {
            while (connected) {
                DatumId datumId;
                {
                    Lock lock{noticeMutex};
                    noticeCond.wait(lock, [&]{return !noticeQ.empty();});

                    datumId = noticeQ.front();
                    noticeQ.pop();
                }

                if (datumId.id == DatumId::Id::DATA_SEG_ID) {
                    LOG_DEBUG("Peer %s is notifying about data-segment %s",
                            to_string().data(), datumId.to_string().data());
                    connected = rpc->notify(datumId.dataSegId);
                }
                else if (datumId.id == DatumId::Id::PROD_INDEX) {
                    LOG_DEBUG("Peer %s is notifying about product %s",
                            to_string().data(), datumId.to_string().data());
                    connected = rpc->notify(datumId.prodId);
                }
                else if (datumId.id == DatumId::Id::TRACKER) {
                    LOG_DEBUG("Peer %s is notifying about tracker %s",
                            to_string().data(), datumId.to_string().data());
                    connected = rpc->notify(datumId.tracker);
                }
                else if (datumId.id == DatumId::Id::PEER_SRVR_ADDR) {
                    LOG_DEBUG("Peer %s is notifying about peer server %s",
                            to_string().data(), datumId.to_string().data());
                    connected = rpc->notify(datumId.tracker);
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

protected:
    SockAddr           rmtSockAddr; ///< Remote socket address
    SockAddr           lclSockAddr; ///< Local (notice) socket address
    std::atomic<bool>  connected;   ///< Connected to remote peer?
    bool               rmtIsPub;    ///< Remote peer is publisher's

    /**
     * Sets the internal thread exception.
     */
    void setExPtr() {
        Guard guard{exceptMutex};
        if (!exPtr)
            exPtr = std::current_exception();
    }

    /**
     * Rethrows the exception thrown by one of this instance's internal threads.
     */
    void throwIf() {
        bool throwEx = false;
        {
            Guard guard{exceptMutex};
            throwEx = static_cast<bool>(exPtr);
        }
        if (throwEx)
            std::rethrow_exception(exPtr);
    }

    void startNoticeWriter() {
        LOG_DEBUG("Starting notice-writer thread");
        noticeWriter = Thread(&PeerImpl::runNoticeWriter, this);
    }
    void stopNoticeWriter() {
        if (noticeWriter.joinable()) {
            ::pthread_cancel(noticeWriter.native_handle());
            noticeWriter.join();
        }
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
     * @param[in] p2pMgr    P2P manager
     * @param[in] rpc       Pointer to RPC module
     * @param[in] isClient  Instance initiated connection?
     * @param[in] isPub     Instance is publisher?
     */
    PeerImpl(
            P2pMgr&    p2pMgr,
            Rpc::Pimpl rpc)
        : exceptMutex()
        , noticeMutex()
        , noticeCond()
        , noticeWriter()
        , p2pMgr(p2pMgr)
        , rpc(rpc)
        , noticeQ()
        , exPtr()
        , rmtSockAddr(rpc->getRmtAddr())
        , lclSockAddr(rpc->getLclAddr())
        , connected{true}
        , rmtIsPub(rpc->isRmtPub())
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
        rpc->start(*this);
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

    bool notify(const Tracker tracker) override {
        throwIf();

        if (connected && !rmtIsPub) {
            //LOG_DEBUG("Peer %s is notifying about tracker %s", to_string().data(),
                    //tracker.to_string().data());
            Guard guard{noticeMutex};
            noticeQ.push(DatumId{tracker});
            noticeCond.notify_all();
        }

        return connected;
    }
    bool notify(const SockAddr srvrAddr) override {
        throwIf();

        if (connected && !rmtIsPub) {
            //LOG_DEBUG("Peer %s is notifying about server %s", to_string().data(),
                    //srvrAddr.to_string().data());
            Guard guard{noticeMutex};
            noticeQ.push(DatumId{srvrAddr});
            noticeCond.notify_all();
        }

        return connected;
    }
    bool notify(const ProdId notice) override {
        throwIf();

        if (connected && !rmtIsPub) {
            //LOG_DEBUG("Peer %s is notifying about product %s", to_string().data(),
                    //notice.to_string().data());
            Guard guard{noticeMutex};
            noticeQ.push(DatumId{notice});
            noticeCond.notify_all();
        }

        return connected;
    }
    bool notify(const DataSegId notice) override {
        throwIf();

        if (connected && !rmtIsPub) {
            //LOG_DEBUG("Peer %s is notifying about data-segment %s", to_string().data(),
                    //notice.to_string().data());
            Guard guard{noticeMutex};
            noticeQ.push(DatumId{notice});
            noticeCond.notify_all();
        }

        return connected;
    }

    /**
     * Processes a request for product information.
     *
     * @param[in] prodId     Index of the product
     */
    bool recvRequest(const ProdId prodId) override {
        LOG_DEBUG("Peer %s received request for information on product %s",
                    to_string().data(), prodId.to_string().data());
        auto prodInfo = p2pMgr.recvRequest(prodId, rmtSockAddr);
        if (prodInfo) {
            LOG_DEBUG("Peer %s is sending information on product %s",
                        to_string().data(), prodId.to_string().data());
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
        auto dataSeg = p2pMgr.recvRequest(dataSegId, rmtSockAddr);
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
        : PeerImpl(p2pMgr, rpc)
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

    bool recvNotice(const ProdId prodId) {
        throw LOGIC_ERROR("Shouldn't have been called");
        return false;
    }
    bool recvNotice(const DataSegId dataSegId) {
        throw LOGIC_ERROR("Shouldn't have been called");
        return false;
    }
    void recvData(const Tracker tracker) {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void recvData(const SockAddr srvrAddr) {
         throw LOGIC_ERROR("Shouldn't have been called");
    }
    void recvData(const ProdInfo prodInfo) {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void recvData(const DataSeg dataSeg) {
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
            DatumId* operator->() {return &request;}
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
         * @param[in] p2pMgr       Subscriber's P2P manager
         * @param[in] rmtSockAddr  Socket address of the remote peer
         * @see SubP2pNode::missed(ProdIndex, SockAddr)
         * @see SubP2pNode::missed(DataSegId, SockAddr)
         */
        void drainTo(
                SubP2pMgr&      p2pMgr,
                const SockAddr rmtSockAddr) {
            Guard guard{mutex};
            while (head) {
                if (head.id == DatumId::Id::PROD_INDEX) {
                    p2pMgr.missed(head.prodId, rmtSockAddr);
                }
                else if (head.id == DatumId::Id::DATA_SEG_ID) {
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
            return Iter{*this, DatumId{}};
        }
    };

    SubP2pMgr&    subP2pMgr; ///< Subscriber's P2P manager
    RequestQ      requested; ///< Requests sent to remote peer

    /**
     * Received a datum from the remote peer. If the datum wasn't requested,
     * then it is ignored and each request in the requested queue before the
     * request for the given datum is removed from the queue and passed to the
     * P2P manager as not satisfiable by the remote peer. Otherwise, the datum
     * is passed to the subscriber's P2P manager and the pending request is
     * removed from the requested queue.
     *
     * @tparam    DATUM       Type of datum: `ProdInfo` or `DataSeg`
     * @param[in] datum       Datum
     * @throw     LogicError  Datum wasn't requested
     * @see `SubP2pMgr::missed()`
     */
    template<typename DATUM>
    void processData(const DATUM datum) {
        const auto& id = datum.getId();
        if (requested.count(DatumId{id}) == 0)
            throw LOGIC_ERROR("Peer " + to_string() + " received "
                    "unrequested product-information " + datum.to_string());

        for (auto iter =  requested.begin(), end = requested.end();
                iter != end; ) {
            if (iter->equals(id)) {
                subP2pMgr.recvData(datum, rmtSockAddr);
                requested.pop();
                break;
            }
            /*
             *  NB: A missed response from the remote peer means that it doesn't
             *  have the requested data.
             */
            if (iter->getType() == DatumId::Id::DATA_SEG_ID) {
                subP2pMgr.missed(iter->dataSegId, rmtSockAddr);
            }
            else if (iter->getType() == DatumId::Id::PROD_INDEX) {
                subP2pMgr.missed(iter->prodId, rmtSockAddr);
            }

            ++iter; // Must occur before `requested.pop()`
            requested.pop();
        }
    }

public:
    /**
     * Constructs.
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
        : PeerImpl(p2pMgr, rpc)
        , subP2pMgr(p2pMgr)
        , requested()
    {}

    /**
     * Constructs client-side.
     *
     * @param[in] p2pMgr        Subscriber's P2P manager
     * @param[in] srvrAddr      Socket address of remote server
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect. Bad failure.
     * @throw     RuntimeError  Couldn't connect. Might be temporary.
     */
    SubPeerImpl(
            SubP2pMgr& p2pMgr,
            SockAddr   srvrAddr)
        : SubPeerImpl(p2pMgr, Rpc::create(srvrAddr))
    {}

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

    /**
     * Receives a notice about available product information. Notifies the
     * subscriber's P2P manager. Requests the datum if told to do so by the P2P
     * manager.
     *
     * @param[in] prodId     Product index
     * @retval    `false`    Connection lost
     * @retval    `true`     Success
     */
    bool recvNotice(const ProdId prodId) {
        LOG_DEBUG("Peer %s received notice about information on product %s",
                to_string().data(), prodId.to_string().data());
        if (subP2pMgr.recvNotice(prodId, rmtSockAddr)) {
            // Subscriber wants the product information
            requested.push(DatumId{prodId});
            LOG_DEBUG("Peer %s is requesting information on product %s",
                    to_string().data(), prodId.to_string().data());
            connected = rpc->request(prodId);
        }
        return connected;
    }

    /**
     * Receives a notice about an available data segment. Notifies the
     * subscriber's P2P manager. Requests the datum if told to do so by the P2P
     * manager.
     *
     * @param[in] dataSegId  Data segment ID
     * @retval    `false`    Connection lost
     * @retval    `true`     Success
     */
    bool recvNotice(const DataSegId dataSegId) {
        LOG_DEBUG("Peer %s received notice about data segment %s",
                to_string().data(), dataSegId.to_string().data());
        if (subP2pMgr.recvNotice(dataSegId, rmtSockAddr)) {
            // Subscriber wants the data segment
            requested.push(DatumId{dataSegId});
            LOG_DEBUG("Peer %s is requesting data segment %s",
                    to_string().data(), dataSegId.to_string().data());
            connected = rpc->request(dataSegId);
        }
        return connected;
    }

    /**
     * Receives tracker information from the remote peer.
     *
     * @param[in] tracker  Socket addresses of potential peer-servers
     */
    void recvData(const Tracker tracker) {
        LOG_DEBUG("Peer %s received tacker %s",
                to_string().data(), tracker.to_string().data());
        subP2pMgr.recvData(tracker, rmtSockAddr);
    }
    /**
     * Receives the address of a potential peer-server
     *
     * @param[in] tracker  Socket addresses of potential peer-server
     */
    void recvData(const SockAddr srvrAddr) {
        LOG_DEBUG("Peer %s received peer-server address %s",
                to_string().data(), srvrAddr.to_string().data());
        subP2pMgr.recvData(srvrAddr, rmtSockAddr);
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
        LOG_DEBUG("Peer %s received data-segment %s",
                to_string().data(), dataSeg.to_string().data());
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
            const SockAddr srvrAddr,
            const bool     iAmPub,
            const unsigned backlog)
        : rpcSrvr(RpcSrvr::create(srvrAddr, iAmPub, backlog))
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
        const SockAddr srvrAddr,
        const unsigned backlog) {
    return Pimpl{new PeerSrvrImpl<PubP2pMgr>(srvrAddr, true, backlog)};
}

template<>
PeerSrvr<SubP2pMgr>::Pimpl PeerSrvr<SubP2pMgr>::create(
        const SockAddr srvrAddr,
        const unsigned backlog) {
    return Pimpl{new PeerSrvrImpl<SubP2pMgr>(srvrAddr, false, backlog)};
}

} // namespace

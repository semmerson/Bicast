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

// Protocol data unit (PDU) identifiers
class PduId : public XprtAble
{
    uint16_t value;

public:
    using Type = decltype(value);

    static constexpr Type UNSET = 0;
    static constexpr Type PEER_SRVR_ADDRS = 1;
    static constexpr Type PUB_PATH_NOTICE = 2;
    static constexpr Type PROD_INFO_NOTICE = 3;
    static constexpr Type DATA_SEG_NOTICE = 4;
    static constexpr Type PROD_INFO_REQUEST = 5;
    static constexpr Type DATA_SEG_REQUEST = 6;
    static constexpr Type PROD_INFO = 7;
    static constexpr Type DATA_SEG = 8;
    static constexpr Type MAX_PDU_ID = DATA_SEG;

    /**
     * Constructs.
     *
     * @param[in] value            PDU ID value
     * @throws    IllegalArgument  `value` is unsupported
     */
    PduId(Type value)
        : value(value)
    {
        if (value > MAX_PDU_ID)
            throw INVALID_ARGUMENT("value=" + to_string());
    }

    PduId()
        : value(UNSET)
    {}

    operator bool() const noexcept {
        return value != UNSET;
    }

    inline String to_string() const {
        return std::to_string(value);
    }

    inline operator Type() const noexcept {
        return value;
    }

    inline bool operator==(const PduId rhs) const noexcept {
        return value == rhs.value;
    }

    inline bool operator==(const Type rhs) const noexcept {
        return value == rhs;
    }

    inline bool write(Xprt xprt) const {
        return xprt.write(value);
    }

    inline bool read(Xprt xprt) {
        return xprt.read(value);
    }
};

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
    P2pMgr&            p2pMgr;        ///< Associated P2P manager
    Rpc::Pimpl         rpc;           ///< Remote procedure call module
    const bool         clientSide;    ///< This instance initiated contact?
    NoticeQ            noticeQ;       ///< Notice queue
    std::exception_ptr exPtr;         ///< Internal thread exception
    const bool         iAmPub;        ///< This instance is publisher's

    void runNoticeWriter(Peer peer) {
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
                    connected = rpc->notify(datumId.prodIndex);
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
    /*
     * If a single transport is used for asynchronous communication and reading
     * and writing occur on the same thread, then deadlock will occur if both
     * receive buffers are full and each end is trying to write. To prevent
     * this, three transports are used and a thread that reads from one
     * transport will write to another. The pipeline might block for a while as
     * messages are processed, but it won't deadlock.
     */
    SockAddr           rmtSockAddr; ///< Remote socket address
    SockAddr           lclSockAddr; ///< Local (notice) socket address
    std::atomic<bool>  connected;   ///< Connected to remote peer?
    bool               rmtIsPub;    ///< Remote peer is publisher's

    /**
     * Constructs.
     *
     * @param[in] p2pMgr  P2P manager
     * @param[in] rpc     Pointer to RPC module
     */
    Impl(   P2pMgr&    p2pMgr,
            Rpc::Pimpl rpc,
            const bool isClient,
            const bool isPub)
        : exceptMutex()
        , noticeMutex()
        , noticeCond()
        , noticeWriter()
        , p2pMgr(p2pMgr)
        , rpc(rpc)
        , clientSide(isClient)
        , noticeQ()
        , exPtr()
        , iAmPub(isPub)
        , rmtSockAddr(rpc->getRmtAddr())
        , lclSockAddr(rpc->getLclAddr())
        , connected{true}
        , rmtIsPub(rpc->isRmtPub())
    {}

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

    void startNoticeWriter(Peer peer) {
        LOG_DEBUG("Starting notice-writer thread");
        noticeWriter = Thread(&Impl::runNoticeWriter, this, peer);
    }
    void stopNoticeWriter() {
        if (noticeWriter.joinable()) {
            ::pthread_cancel(noticeWriter.native_handle());
            noticeWriter.join();
        }
    }

    /**
     * Stops and joins the threads that are reading and servicing messages from
     * the remote peer.
     *
     * Idempotent
     */
    void stopThreads() {
        stopNoticeWriter();
    }

public:
    Impl(const Impl& impl) =delete; // Rule of three

    virtual ~Impl() noexcept
    {
        stopThreads();
    }

    Impl& operator=(const Impl& rhs) noexcept =delete; // Rule of three

    bool isClient() const noexcept {
        return clientSide;
    }

    bool isRmtPub() const noexcept {
        return rmtIsPub;
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
        rpc->stop();
    }

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() const noexcept {
        return rmtSockAddr;
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
        return "{lcl=" + lclSockAddr.to_string() + ", rmt=" +
                rmtSockAddr.to_string() + "}";
    }

    bool notify(const Tracker tracker) {
        throwIf();

        if (connected && !rmtIsPub) {
            Guard guard{noticeMutex};
            noticeQ.push(DatumId{tracker});
            noticeCond.notify_all();
        }

        return connected;
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

    /**
     * Processes a request for product information.
     *
     * @param[in] prodIndex     Index of the product
     */
    void recvRequest(const ProdIndex prodIndex) {
        LOG_DEBUG("Peer %s received request for information on product %s",
                    to_string().data(), prodIndex.to_string().data());
        auto prodInfo = p2pMgr.recvRequest(prodIndex, rmtSockAddr);
        if (prodInfo)
            rpc->send(prodInfo);
    }

    /**
     * Processes a request for a data segment.
     *
     * @param[in] dataSegId     Data segment ID
     */
    void recvRequest(const DataSegId dataSegId) {
        LOG_DEBUG("Peer %s received request for data segment %s",
                    to_string().data(), dataSegId.to_string().data());
        auto dataSeg = p2pMgr.recvRequest(dataSegId, rmtSockAddr);
        if (dataSeg)
            rpc->send(dataSeg);
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

SockAddr Peer::getRmtAddr() noexcept {
    return pImpl->getRmtAddr();
}

void Peer::start() const {
    throw LOGIC_ERROR("Shouldn't have been called");
}

void Peer::stop() const {
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

bool Peer::notify(const Tracker tracker) const {
    return pImpl->notify(tracker);
}

bool Peer::notify(const ProdIndex notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const DataSegId notice) const {
    return pImpl->notify(notice);
}

void Peer::recvRequest(const ProdIndex prodIndex) const {
    pImpl->recvRequest(prodIndex);
}

void Peer::recvRequest(const DataSegId dataSegId) const {
    pImpl->recvRequest(dataSegId);
}


/******************************************************************************/

/**
 * Publisher's peer implementation. The peer will be constructed server-side.
 */
class PubPeerImpl final : public Peer::Impl
{
    PubRpc::Pimpl rpc;

public:
    /**
     * Constructs server-side.
     *
     * @param[in] node          Publisher's P2P node
     * @param[in] xprt          Transport connected to the remote peer
     * @throw     RuntimeError  Lost connection
     */
    PubPeerImpl(P2pMgr& p2pMgr, PubRpc::Pimpl rpc)
        : Impl(p2pMgr, rpc, false, true)
        , rpc{rpc}
    {}

    void start(PubPeer peer) {
        rpc->start(peer);
    }
};

PubPeer::PubPeer(P2pMgr& p2pMgr, PubRpc::Pimpl rpc)
    : Peer(new PubPeerImpl(p2pMgr, rpc))
{}

void PubPeer::start() const {
    static_cast<PubPeerImpl*>(pImpl.get())->start(*this);
}

/******************************************************************************/

/**
 * Subscriber's peer implementation. May be constructed server-side or
 * client-side.
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
                    p2pMgr.missed(head.prodIndex, rmtSockAddr);
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
    SubRpc::Pimpl rpc;       ///< Subscriber remote-procedure-call module

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
                subP2pMgr.missed(iter->prodIndex, rmtSockAddr);
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
            SubP2pMgr&    p2pMgr,
            SubRpc::Pimpl rpc,
            const bool    isClient)
        : Impl(p2pMgr, rpc, isClient, false)
        , subP2pMgr(p2pMgr)
        , requested()
        , rpc(rpc)
    {}

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
        : SubPeerImpl(p2pMgr, SubRpc::create(srvrAddr), true)
    {}

    /**
     * Server-side construction.
     *
     * @param[in] p2pMgr  Subscriber's P2P manager
     * @param[in] rpc     Pointer to RPC module
     */
    SubPeerImpl(
            SubP2pMgr&    p2pMgr,
            SubRpc::Pimpl rpc)
        : SubPeerImpl(p2pMgr, rpc, false)
    {}

    void start(SubPeer peer) {
        rpc->start(peer);
    }

    /**
     * Receives a notice about available product information . Notifies the
     * subscriber's P2P manager. Requests the datum if told to do so by the P2P
     * manager.
     *
     * @param[in] prodIndex  Product index
     * @retval    `false`    Connection lost
     * @retval    `true`     Success
     */
    bool recvNotice(const ProdIndex prodIndex) {
        LOG_DEBUG("Peer %s received notice about information on product %s",
                to_string().data(), prodIndex.to_string().data());
        if (subP2pMgr.recvNotice(prodIndex, rmtSockAddr)) {
            // Subscriber wants the product information
            requested.push(DatumId{prodIndex});
            connected = rpc->request(prodIndex);
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
        LOG_DEBUG("Peer %s received product information %s",
                to_string().data(), prodInfo.to_string().data());
        processData<ProdInfo>(prodInfo);
    }
    /**
     * Receives a data segment from the remote peer.
     *
     * @param[in] dataSeg  Data segment
     */
    void recvData(const DataSeg dataSeg) {
        LOG_DEBUG("Peer %s received product information %s",
                to_string().data(), dataSeg.to_string().data());
        processData<DataSeg>(dataSeg);
    }
};

SubPeer::SubPeer(
        SubP2pMgr&    p2pMgr,
        SubRpc::Pimpl rpc)
    : Peer(new SubPeerImpl(p2pMgr, rpc))
{}

SubPeer::SubPeer(SubP2pMgr&      p2pMgr,
                 const SockAddr& srvrAddr)
    : Peer(new SubPeerImpl(p2pMgr, srvrAddr))
{}

void SubPeer::start() const {
    static_cast<SubPeerImpl*>(pImpl.get())->start(*this);
}

void SubPeer::recvNotice(const ProdIndex prodIndex) const {
    static_cast<SubPeerImpl*>(pImpl.get())->recvNotice(prodIndex);
}

void SubPeer::recvNotice(const DataSegId dataSegId) const {
    static_cast<SubPeerImpl*>(pImpl.get())->recvNotice(dataSegId);
}

void SubPeer::recvData(const Tracker tracker) const {
    static_cast<SubPeerImpl*>(pImpl.get())->recvData(tracker);
}

void SubPeer::recvData(const SockAddr srvrAddr) const {
    static_cast<SubPeerImpl*>(pImpl.get())->recvData(srvrAddr);
}

void SubPeer::recvData(const ProdInfo prodInfo) const {
    static_cast<SubPeerImpl*>(pImpl.get())->recvData(prodInfo);
}

void SubPeer::recvData(const DataSeg dataSeg) const {
    static_cast<SubPeerImpl*>(pImpl.get())->recvData(dataSeg);
}

/******************************************************************************/

template<typename P2P_MGR>
class PeerSrvrImpl : public PeerSrvr<P2P_MGR>
{
public:
    using Peer    = typename P2P_MGR::PeerType;
    using RpcSrvr = typename Peer::RpcSrvrType;

    PeerSrvrImpl(
            const SockAddr srvrAddr,
            const unsigned backlog)
        : rpcSrvr(RpcSrvr::create(srvrAddr, backlog))
    {}

    SockAddr getSrvrAddr() const override {
        return rpcSrvr->getSrvrAddr();
    }

    Peer accept(P2P_MGR& p2pMgr) const override {
        auto rpc = rpcSrvr->accept();
        return Peer{p2pMgr, rpc};
    }

private:
    typename RpcSrvr::Pimpl rpcSrvr;
};

template<>
PeerSrvr<P2pMgr>::Pimpl PeerSrvr<P2pMgr>::create(
        const SockAddr srvrAddr,
        const unsigned backlog) {
    return Pimpl{new PeerSrvrImpl<P2pMgr>(srvrAddr, backlog)};
}

template<>
PeerSrvr<SubP2pMgr>::Pimpl PeerSrvr<SubP2pMgr>::create(
        const SockAddr srvrAddr,
        const unsigned backlog) {
    return Pimpl{new PeerSrvrImpl<SubP2pMgr>(srvrAddr, backlog)};
}

} // namespace

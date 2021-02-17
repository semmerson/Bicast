/**
 * Peer-to-peer connection protocol.
 *
 *        File: PeerProto.cpp
 *  Created on: Nov 5, 2019
 *      Author: Steven R. Emmerson
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

#include "PeerProto.h"

#include "error.h"
#include "protocol.h"
#include "Thread.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <pthread.h>
#include <thread>

namespace hycast {

/**
 * Abstract base class for implementing the peer protocol.
 */
class PeerProto::Impl
{
protected:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;
    typedef std::exception_ptr      ExceptPtr;
    typedef std::atomic_flag        AtomicFlag;
    typedef uint16_t                MsgIdType;

    AtomicFlag    executing;
    ExceptPtr     exPtr;
    mutable Mutex doneMutex;
    mutable Cond  doneCond;
    TcpSock       noteSock;       ///< For exchanging notices
    TcpSock       srvrSock;       ///< Receiving requests and sending chunks
    std::string   string;         ///< `to_string()` string
    SendPeer&     sendPeer;       ///< Sending peer
    bool          done;           ///< Halt requested or exception thrown?
    std::thread   rcvReqstThread; ///< Requests received and processed
    NodeType      lclNodeType;    ///< Type of local node
    NodeType      rmtNodeType;    ///< Type of remote node

    static const unsigned char PROTO_VERSION = 1;
    static const MsgIdType     PROD_INFO_NOTICE = MsgId::PROD_INFO_NOTICE;
    static const MsgIdType     DATA_SEG_NOTICE = MsgId::DATA_SEG_NOTICE;
    static const MsgIdType     PROD_INFO_REQUEST = MsgId::PROD_INFO_REQUEST;
    static const MsgIdType     DATA_SEG_REQUEST = MsgId::DATA_SEG_REQUEST;
    static const MsgIdType     PROD_INFO = MsgId::PROD_INFO;
    static const MsgIdType     DATA_SEG = MsgId::DATA_SEG;
    static const MsgIdType     PATH_TO_SRC = MsgId::PATH_TO_PUB;
    static const MsgIdType     NO_PATH_TO_SRC = MsgId::NO_PATH_TO_PUB;

    void init()
    {
        // Configure the notice socket
        noteSock.setDelay(false);

        // Exchange node types
        MsgIdType typeInt = lclNodeType;
        LOG_DEBUG("Writing node-type " + std::to_string(typeInt));
        noteSock.write(typeInt);

        if (!noteSock.read(typeInt))
            throw RUNTIME_ERROR("EOF");
        LOG_DEBUG("Read node-type " + std::to_string(typeInt));
        rmtNodeType = static_cast<NodeType>(typeInt);
    }

    void setExcept(const std::exception& ex)
    {
        Guard guard(doneMutex);

        if (!exPtr) {
            LOG_DEBUG("Setting exception");
            exPtr = std::make_exception_ptr(ex);
            doneCond.notify_all();
        }
    }

    /**
     * Waits until this instance is done. Rethrows subtask exception if
     * appropriate.
     */
    void waitUntilDone()
    {
        Lock lock(doneMutex);

        while (!done && !exPtr)
            doneCond.wait(lock);

        if (!done && exPtr)
            std::rethrow_exception(exPtr);
    }

    bool recvProdReq()
    {
        ProdIndex::Type prodIndex;

        if (!srvrSock.read(prodIndex)) // Performs network translation
            return false;
        LOG_DEBUG("Received request for product " + std::to_string(prodIndex));

        int    cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            sendPeer.sendMe(prodIndex);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    bool recvSegReq()
    {
        try {
            ProdIndex::Type prodIndex;

            if (!srvrSock.read(prodIndex)) // Performs network translation
                return false;

            ProdSize  segOffset;

            if (!srvrSock.read(segOffset))
                return false;

            SegId segId{prodIndex, segOffset};
            LOG_DEBUG("Received request for data-segment " + segId.to_string());
            {
                //Canceler canceler{false}; // Disable thread cancellation
                sendPeer.sendMe(segId);
            }

            return true;
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught exception \"%s\"", ex.what());
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    /**
     * Returns on EOF.
     */
    void recvRequests()
    {
        try {
            LOG_DEBUG("Receiving requests");

            for (;;) {
                MsgIdType msgId;

                if (!srvrSock.read(msgId)) // Performs network translation
                    break; // EOF

                if (msgId == PROD_INFO_REQUEST) {
                    if (!recvProdReq())
                        break;
                }
                else if (msgId == DATA_SEG_REQUEST) {
                    if (!recvSegReq())
                        break;
                }
                else {
                    throw RUNTIME_ERROR("Invalid message ID: " +
                            std::to_string(msgId));
                }
            }

            halt();
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught exception \"%s\"", ex.what());
            setExcept(ex);
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    /**
     * Starts the thread on which requests from the remote peer are received and
     * processed.
     *
     * This function exists because calling `std::thread(&Impl::recvRequests,
     * this)` in `PubPeerProto`, for example, is illegal because
     * `recvRequests()` is protected. (Doesn't make sense to me.)
     *
     * @throws RuntimeError  Thread couldn't be created
     */
    void startReqstThread()
    {
        try {
            /*
             * The task isn't detached because then it couldn't be canceled
             * (`std::thread::native_handle()` returns 0 for detached threads).
             */
            //LOG_DEBUG("Creating thread for receiving requests");
            rcvReqstThread = std::thread(&Impl::recvRequests, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread for "
                    "receiving requests"));
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    virtual void startTasks() =0;

    /**
     * Idempotent.
     */
    virtual void stopTasks() =0;

    /**
     * Disconnects from the remote peer. Idempotent.
     */
    virtual void disconnect() =0;

    static TcpSrvrSock createSrvrSock(
            InetAddr  inAddr,
            const int queueSize)
    {
        try {
            auto srvrAddr = inAddr.getSockAddr(0); // O/S assigned port number
            return TcpSrvrSock(srvrAddr, queueSize);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't create server socket"));
        }

    }

    void send(
            const MsgIdType msgId,
            const ProdIndex prodIndex,
            TcpSock&        sock)
    {
        LOG_DEBUG("Sending product-index " + prodIndex.to_string());
        // The following perform network translation
        sock.write(msgId);
        sock.write(prodIndex.getValue());
    }

    void send(
            const MsgIdType msgId,
            const SegId&    id,
            TcpSock&        sock)
    {
        LOG_DEBUG("Sending segment-ID " + id.to_string());
        // The following perform network translation
        sock.write(msgId);
        sock.write(id.getProdIndex().getValue());
        sock.write(id.getOffset());
    }

public:
    /**
     * Server-side construction (i.e., from an `::accept()`). Applicable to both
     * a publisher and a subscriber.
     *
     * @param[in] sock         `::accept()`ed TCP socket
     * @param[in] lclNodeType  Type of local node
     * @param[in] peer         Sending peer
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      Yes
     */
    Impl(   TcpSock&       sock,
            const NodeType lclNodeType,
            SendPeer&      peer)
        : executing{false}
        , exPtr{}
        , doneMutex{}
        , doneCond{}
        , noteSock(sock)
        , srvrSock{}
        , string{}
        , sendPeer(peer)
        , done{false}
        , rcvReqstThread{}
        , lclNodeType{lclNodeType}
        , rmtNodeType{}
    {
        init();

        /*
         * Temporary server for creating a socket for receiving requests and
         * sending chunks.
         */
        auto tmpSrvrSock = createSrvrSock(sock.getLclAddr().getInetAddr(), 1);

        /*
         * Create a socket for receiving requests and sending chunks
         */
        in_port_t port = tmpSrvrSock.getLclPort();
        LOG_DEBUG("Writing port " + std::to_string(port));
        noteSock.write(port);
        // TODO: Ensure connection is from remote peer
        LOG_DEBUG("Accepting server socket");
        srvrSock = TcpSock{tmpSrvrSock.accept()};
        srvrSock.setDelay(true); // Consolidate ACK and chunk
    }

    /**
     * Client-side construction. Applicable to a subscriber only.
     *
     * @param[in] sock         Socket
     * @param[in] lclNodeType  Type of local node
     * @param[in] peer         Peer
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      Yes
     */
    Impl(   TcpSock&&      sock,
            const NodeType lclNodeType,
            SendPeer&      peer)
        : executing{false}
        , exPtr{}
        , doneMutex{}
        , doneCond{}
        , noteSock(sock)
        , srvrSock{}
        , string{}
        , sendPeer(peer)
        , done{false}
        , rcvReqstThread{}
        , lclNodeType{lclNodeType}
        , rmtNodeType{}
    {
        if (lclNodeType == NodeType::PUBLISHER)
            throw LOGIC_ERROR("Publisher can't be client");

        init();
    }

    virtual ~Impl() noexcept =default;

    NodeType getRmtNodeType() const noexcept
    {
        return rmtNodeType;
    }

    SockAddr getRmtAddr() const
    {
        return noteSock.getRmtAddr();
    }

    SockAddr getLclAddr() const
    {
        return noteSock.getLclAddr();
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
        try {
            if (executing.test_and_set())
                throw LOGIC_ERROR("Already called");

            startTasks();

            try {
                waitUntilDone();
                stopTasks(); // Idempotent
                disconnect(); // Idempotent
            }
            catch (...) {
                stopTasks(); // Idempotent
                disconnect(); // Idempotent
                throw;
            }
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Failure"));
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

        LOG_DEBUG("Halt requested");

        done = true;
        doneCond.notify_all();
    }

    void notify(const ProdIndex prodIndex)
    {
        send(PROD_INFO_NOTICE, prodIndex, noteSock);
    }

    void notify(const SegId& id)
    {
        send(DATA_SEG_NOTICE, id, noteSock);
    }

    void send(const ProdInfo& info)
    {
        const std::string& name = info.getProdName();
        const SegSize      nameLen = name.length();

        LOG_DEBUG("Sending product-information %s",
                info.to_string().data());

        // The following perform host-to-network translation
        srvrSock.write(PROD_INFO);
        srvrSock.write(nameLen);
        srvrSock.write(info.getProdIndex().getValue());
        srvrSock.write(info.getProdSize());

        srvrSock.write(name.data(), nameLen); // No network translation
    }

    void send(const MemSeg& seg)
    {
        try {
            const SegInfo& info = seg.getSegInfo();
            SegSize        segSize = info.getSegSize();

            LOG_DEBUG("Sending segment %s", info.to_string().data());

            // The following perform host-to-network translation
            srvrSock.write(DATA_SEG);
            srvrSock.write(segSize);
            srvrSock.write(info.getSegId().getProdIndex().getValue());
            srvrSock.write(info.getProdSize());
            srvrSock.write(info.getSegId().getOffset());

            // No network translation
            srvrSock.write(seg.data(), segSize);
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught exception \"%s\"", ex.what());
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    /**
     * Notifies the remote peer that this local node just transitioned to being
     * a path to the publisher of data-products.
     */
    virtual void gotPath() =0;

    /**
     * Notifies the remote peer that this local node just transitioned to not
     * being a path to the publisher of data-products.
     */
    virtual void lostPath() =0;

    /**
     * Requests information on a product from the remote peer.
     *
     * @param[in] prodIndex  Product index
     * @cancellationpoint    Yes
     */
    virtual void request(ProdIndex prodIndex) =0;

    /**
     * Requests a data-segment from the remote peer.
     *
     * @param[in] segId      Segment identifier
     * @cancellationpoint    Yes
     */
    virtual void request(SegId segId) =0;
};

PeerProto::operator bool() const {
    return pImpl.operator bool();
}

NodeType PeerProto::getRmtNodeType() const noexcept {
    return pImpl->getRmtNodeType();
}

SockAddr PeerProto::getRmtAddr() const {
    return pImpl->getRmtAddr();
}

SockAddr PeerProto::getLclAddr() const {
    return pImpl->getLclAddr();
}

std::string PeerProto::to_string() const {
    return pImpl->to_string();
}

void PeerProto::operator()() const {
    pImpl->operator()();
}

void PeerProto::halt() const {
    pImpl->halt();
}

void PeerProto::notify(const ProdIndex prodId) const {
    pImpl->notify(prodId);
}

void PeerProto::notify(const SegId& id) const {
    pImpl->notify(id);
}

void PeerProto::send(const ProdInfo& prodInfo) const {
    pImpl->send(prodInfo);
}

void PeerProto::send(const MemSeg& memSeg) const {
    pImpl->send(memSeg);
}

void PeerProto::request(const ProdIndex prodIndex) const {
    pImpl->request(prodIndex);
}

void PeerProto::request(const SegId segId) const {
    pImpl->request(segId);
}

void PeerProto::gotPath() const {
    pImpl->gotPath();
}

void PeerProto::lostPath() const {
    pImpl->lostPath();
}

/******************************************************************************/

/**
 * Publisher's peer-protocol implementation.
 */
class PubPeerProto final : public PeerProto::Impl
{
protected:
    void startTasks() override
    {
        startReqstThread();
    }

    /**
     * Idempotent.
     */
    void stopTasks() override
    {
        if (rcvReqstThread.joinable()) {
            srvrSock.shutdown();
            rcvReqstThread.join();
        }
    }

    /**
     * Disconnects from the remote peer. Idempotent.
     */
    void disconnect() override
    {
        try {
            srvrSock.shutdown();
            noteSock.shutdown();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Failure"));
        }
    }

public:
    /**
     * Constructs.
     *
     * @param[in] sock         `::accept()`ed TCP socket
     * @param[in] peer         Peer
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      Yes
     */
    PubPeerProto(
            TcpSock&  sock,
            SendPeer& peer)
        : PeerProto::Impl(sock, NodeType::PUBLISHER, peer)
    {}

    ~PubPeerProto() noexcept override
    {
        try {
            stopTasks();  // Idempotent
            disconnect(); // Idempotent
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Failure");
        }
    }

    void gotPath()
    {
        throw LOGIC_ERROR("Invalid action for a publisher");
    }

    void lostPath()
    {
        throw LOGIC_ERROR("Invalid action for a publisher");
    }

    void request(ProdIndex prodIndex)
    {
        throw LOGIC_ERROR("Invalid action for a publisher");
    }

    void request(SegId segId)
    {
        throw LOGIC_ERROR("Invalid action for a publisher");
    }
};

PeerProto::PeerProto(
        TcpSock&  sock,
        SendPeer& peer)
    : pImpl(new PubPeerProto(sock, peer)) {
}

/******************************************************************************/

/**
 * Subscriber's peer-protocol implementation
 */
class SubPeerProto final : public PeerProto::Impl
{
protected:
    TcpSock     clntSock;       ///< Requesting and receiving chunks
    RecvPeer&   recvPeer;       ///< Receiving peer
    std::thread rcvNoteThread;  ///< Receiving and processing notices
    std::thread rcvChunkThread; ///< Receiving and processing chunks

    ProdIndex recvProdIndex()
    {
        ProdIndex::Type index;

        return noteSock.read(index)
            ? ProdIndex(index)
            : ProdIndex{};
    }

    bool recvProdNotice()
    {
        //LOG_DEBUG("Receiving product-information notice");
        const auto prodIndex = recvProdIndex();
        if (!prodIndex)
            return false;

        LOG_DEBUG("Received product-index " + prodIndex.to_string());

        int cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            recvPeer.available(prodIndex);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    bool recvSegNotice()
    {
        //LOG_DEBUG("Receiving data-segment notice");
        const auto prodIndex = recvProdIndex();
        if (!prodIndex)
            return false;

        ProdSize segOffset;
        if (!noteSock.read(segOffset))
            return false;

        const SegId segId(prodIndex, segOffset);
        LOG_DEBUG("Received segment-ID " + segId.to_string());

        int   cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            recvPeer.available(segId);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    /**
     * Returns on EOF;
     */
    void recvNotices()
    {
        try {
            //LOG_DEBUG("Receiving notices");

            for (;;) {
                MsgIdType msgId;

                if (!noteSock.read(msgId)) // Performs network translation
                    break; // EOF

                if (msgId == PATH_TO_SRC) {
                    recvPeer.pathToPub();
                }
                else if (msgId == NO_PATH_TO_SRC) {
                    recvPeer.noPathToPub();
                }
                else if (msgId == PROD_INFO_NOTICE) {
                    if (!recvProdNotice())
                        break;
                }
                else if (msgId == DATA_SEG_NOTICE) {
                    if (!recvSegNotice())
                        break;
                }
                else {
                    throw RUNTIME_ERROR("Invalid message ID: " +
                            std::to_string(msgId));
                }
            }

            halt();
        }
        catch (const std::exception& ex) {
            setExcept(ex);
        }
    }

    bool recvCommon(
            ProdIndex& prodIndex,
            ProdSize&  prodSize,
            SegSize&   segSize)
    {
        ProdIndex::Type index;

        if (!clntSock.read(segSize) || !clntSock.read(index) ||
                !clntSock.read(prodSize))
            return false;

        prodIndex = ProdIndex(index);

        return true;
    }

    bool recvProdInfo()
    {
        ProdIndex prodIndex;
        ProdSize  prodSize;
        SegSize   nameLen;

        if (!recvCommon(prodIndex, prodSize, nameLen))
            return false;

        char buf[nameLen];

        if (!clntSock.read(buf, nameLen))
            return false;

        std::string name(buf, nameLen);
        ProdInfo    prodInfo{prodIndex, prodSize, name};
        int         cancelState;

        LOG_DEBUG("Received product-information " + prodInfo.to_string());

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            recvPeer.hereIs(prodInfo);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    bool recvDataSeg()
    {
        ProdIndex prodIndex;
        ProdSize  prodSize;
        SegSize   dataLen;

        if (!recvCommon(prodIndex, prodSize, dataLen))
            return false; // EOF

        ProdSize segOffset;
        if (!clntSock.read(segOffset))
            return false; // EOF

        SegId   segId{prodIndex, segOffset};
        SegInfo segInfo{segId, prodSize, dataLen};
        TcpSeg  seg{segInfo, clntSock};
        int     cancelState;

        LOG_DEBUG("Received data-segment " + seg.getSegId().to_string());

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            recvPeer.hereIs(seg);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    /**
     * Returns on EOF;
     */
    void recvChunks()
    {
        try {
            LOG_DEBUG("Receiving chunks");

            for (;;) {
                MsgIdType msgId;

                // The following performs network translation
                if (!clntSock.read(msgId))
                    break; // EOF

                if (msgId == PROD_INFO) {
                    if (!recvProdInfo())
                        break; // EOF
                }
                else if (msgId == DATA_SEG) {
                    if (!recvDataSeg())
                        break; // EOF
                }
                else {
                    throw RUNTIME_ERROR("Invalid message ID: " +
                            std::to_string(msgId));
                }
            }

            halt();
        }
        catch (const std::exception& ex) {
            setExcept(ex);
        }
    }

    void startTasks() override
    {
        try {
            /*
             * The tasks aren't detached because then they couldn't be canceled
             * (`std::thread::native_handle()` returns 0 for detached threads).
             */
            //LOG_DEBUG("Creating thread for receiving chunks");
            rcvChunkThread = std::thread(&SubPeerProto::recvChunks, this);

            //LOG_DEBUG("Creating thread for receiving notices");
            rcvNoteThread = std::thread(&SubPeerProto::recvNotices, this);

            if (rmtNodeType != NodeType::PUBLISHER)
                startReqstThread();
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");

            if (rcvNoteThread.joinable()) {
                ::pthread_cancel(rcvNoteThread.native_handle());
                rcvNoteThread.join();
            }
            if (rcvChunkThread.joinable()) {
                ::pthread_cancel(rcvChunkThread.native_handle());
                rcvChunkThread.join();
            }

            throw;
        }
    }

    /**
     * Idempotent.
     */
    void stopTasks() override
    {
        int status;

        if (rcvReqstThread.joinable()) {
            srvrSock.shutdown();
            rcvReqstThread.join();
        }
        if (rcvNoteThread.joinable()) {
            noteSock.shutdown();
            rcvNoteThread.join();
        }
        if (rcvChunkThread.joinable()) {
            clntSock.shutdown();
            rcvChunkThread.join();
        }
    }

    /**
     * Disconnects from the remote peer. Idempotent.
     */
    void disconnect() override
    {
        try {
            if (srvrSock)
                srvrSock.shutdown();
            clntSock.shutdown();
            noteSock.shutdown();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Failure"));
        }
    }

public:
    /**
     * Server-side construction (i.e., from a socket returned by `::accept()`).
     * Remote peer can't be the publisher because publishers don't call
     * `::connect()`.
     *
     * @param[in] sock         `::accept()`ed TCP socket
     * @param[in] subPeer      Subscriber peer
     * @exceptionsafety        Strong guarantee
     * @cancellationpoint      Yes
     */
    SubPeerProto(
            TcpSock&    sock,
            NodeType    lclNodeType,
            RecvPeer&   recvPeer)
        : PeerProto::Impl(sock, lclNodeType,
                //recvPeer.asSendPeer())
                *reinterpret_cast<SendPeer*>(&recvPeer))
        , clntSock{}
        , recvPeer(recvPeer)
        , rcvNoteThread{}
        , rcvChunkThread{}
    {
        /*
         * Temporary server for creating a socket for sending requests and
         * receiving chunks.
         */
        TcpSrvrSock tmpSrvrSock(createSrvrSock(sock.getLclAddr().getInetAddr(),
                1));

        // Create socket for sending requests and receiving chunks
        noteSock.write(tmpSrvrSock.getLclPort());
        // TODO: Ensure connections are from remote peer
        clntSock = TcpSock{tmpSrvrSock.accept()};
        clntSock.setDelay(false); // Requests are immediately sent
    }

    /**
     * Client-side construction (i.e., calls `::connect()`).
     *
     * @param[in] rmtSrvrAddr         Socket address of the remote peer-server
     * @param[in] lclNodeType         Type of local node
     * @param[in] subPeer             Subscriber peer
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @throws    LogicError          `lclNodeType == NodeType::PUBLISHER`
     * @exceptionsafety               Strong guarantee
     * @cancellationpoint             Yes
     */
    SubPeerProto(
            const SockAddr& rmtSrvrAddr,
            const NodeType  lclNodeType,
            RecvPeer&       recvPeer)
        : PeerProto::Impl(TcpClntSock(rmtSrvrAddr), lclNodeType,
                //recvPeer.asSendPeer())
                *reinterpret_cast<SendPeer*>(&recvPeer))
        , clntSock{}
        , recvPeer(recvPeer)
        , rcvNoteThread{}
        , rcvChunkThread{}
    {
        if (lclNodeType == NodeType::PUBLISHER)
            throw LOGIC_ERROR("Publisher can't be subscriber");

        // Create socket for sending requests and receiving chunks
        in_port_t port;
        if (!noteSock.read(port))
            throw RUNTIME_ERROR("EOF");
        LOG_DEBUG("Read port " + std::to_string(port));
        clntSock = TcpClntSock(rmtSrvrAddr.clone(port));

        try {
            clntSock.setDelay(false);

            if (rmtNodeType != NodeType::PUBLISHER) {
                /*
                 * Create socket for receiving requests and sending chunks
                 */
                in_port_t port;
                if (!noteSock.read(port))
                    throw RUNTIME_ERROR("EOF");
                LOG_DEBUG("Read port " + std::to_string(port));
                srvrSock = TcpClntSock(rmtSrvrAddr.clone(port));
                srvrSock.setDelay(true); // Consolidate ACK and chunk
            }
        }
        catch (const std::exception& ex) {
            clntSock.shutdown();
            throw;
        }
    }

    ~SubPeerProto() noexcept
    {
        try {
            stopTasks();  // Idempotent
            disconnect(); // Idempotent
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Failure");
        }
    }

    void gotPath()
    {
        if (rmtNodeType != NodeType::PUBLISHER)
            noteSock.write(PATH_TO_SRC);
    }

    void lostPath()
    {
        if (rmtNodeType != NodeType::PUBLISHER)
            noteSock.write(NO_PATH_TO_SRC);
    }

    void notify(const ProdIndex prodIndex)
    {
        if (rmtNodeType != NodeType::PUBLISHER)
            send(PROD_INFO_NOTICE, prodIndex, noteSock);
    }

    void notify(const SegId& id)
    {
        if (rmtNodeType != NodeType::PUBLISHER)
            send(DATA_SEG_NOTICE, id, noteSock);
    }

    void request(const ProdIndex prodIndex)
    {
        send(PROD_INFO_REQUEST, prodIndex, clntSock);
    }

    void request(const SegId segId)
    {
        send(DATA_SEG_REQUEST, segId, clntSock);
    }
};

PeerProto::PeerProto(
        TcpSock&    sock,
        NodeType    lclNodeType,
        RecvPeer&   subPeer)
    : pImpl(new SubPeerProto(sock, lclNodeType, subPeer)) {
}

PeerProto::PeerProto(
        const SockAddr& rmtSrvrAddr,
        const NodeType  lclNodeType,
        RecvPeer&       subPeer)
    : pImpl(new SubPeerProto(rmtSrvrAddr, lclNodeType, subPeer)) {
}

PeerProto::PeerProto(const PeerProto& peerProto) noexcept
    : pImpl{peerProto.pImpl} {
}

PeerProto::PeerProto(PeerProto&& peerProto) noexcept
    : pImpl{peerProto.pImpl} {
}

PeerProto& PeerProto::operator=(PeerProto&& peerProto) noexcept {
    pImpl = peerProto.pImpl;
    return *this;
}

} // namespace

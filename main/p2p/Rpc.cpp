/**
 * This file implements a P2P RPC layer.
 *
 *  @file:  Rpc.cpp
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

#include "Rpc.h"

#include "error.h"
#include "HycastProto.h"
#include "Peer.h"
#include "ThreadException.h"

#include <array>
#include <list>
#include <memory>
#include <poll.h>
#include <queue>
#include <semaphore.h>
#include <sstream>
#include <unordered_map>

namespace hycast {

using XprtArray = std::array<Xprt, 3>;

/**
 * Implements a P2P RPC layer.
 */
class RpcImpl final : public Rpc
{
    enum class State {
        INIT,
        STARTED,
        STOPPED
    }                  state;         ///< State of this instance
    ThreadEx           threadEx;      ///< Internal thread exception
    mutable Mutex      stateMutex;    ///< Protects state changes
    mutable sem_t      stopSem;       ///< For async-signal-safe stopping
    const bool         iAmClient;     ///< This instance initiated the connection?
    const bool         iAmPub;        ///< This instance is publisher?
    bool               rmtIsPub;      ///< Remote peer is publisher?
    bool               started;       ///< This instance has been started?
    /*
     * If a single transport is used for asynchronous communication and reading
     * and writing occur on the same thread, then deadlock will occur if both
     * receive buffers are full and each end is trying to write. To prevent
     * this, three transports are used and a thread that reads from one
     * transport will write to another. The pipeline might block for a while as
     * messages are processed, but it won't deadlock.
     */
    Xprt               noticeXprt;    ///< Notice transport
    Xprt               requestXprt;   ///< Request and tracker information transport
    Xprt               dataXprt;      ///< Data transport
    SockAddr           rmtSockAddr;   ///< Remote (notice) socket address
    SockAddr           lclSockAddr;   ///< Local (notice) socket address
    Thread             requestReader; ///< For receiving requests
    Thread             noticeReader;  ///< For receiving notices
    Thread             dataReader;    ///< For receiving data

#if 0
    /**
     * Connects this client-side instance to its remote peer. Called by constructor.
     *
     * @param[in] srvrAddr      Address of remote RPC server
     * @param[in] timeout       Timeout, in ms, for connecting with remote peer. <=0 => system's
     *                          default timeout;
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect
     */
    void connect(const SockAddr srvrAddr, int timeout) {
        if (timeout <= 0)
            timeout = -1;

        /*
         * Keep consonant with `setXprts()`.
         */
        noticeXprt      = Xprt{TcpClntSock{srvrAddr, timeout}};

        const auto port = noticeXprt.getLclAddr().getPort();
        uint8_t    xprtId = 0;
        bool       success = noticeXprt.write(port) && noticeXprt.write(xprtId++);

        if (success) {
            requestXprt = Xprt{TcpClntSock{srvrAddr}};
            success = requestXprt.write(port) && requestXprt.write(xprtId++);

            if (success) {
                dataXprt = Xprt{TcpClntSock{srvrAddr}};
                success = dataXprt.write(port) && dataXprt.write(xprtId++);
            }
        }

        if (!success)
            throw RUNTIME_ERROR("Couldn't connect to " + srvrAddr.to_string());
    }
#else
    static void asyncConnectXprt(
            const SockAddr srvrAddr,
            Xprt&          xprt,
            const int      timeout) {
        try {
            const auto  srvrInetAddr = srvrAddr.getInetAddr();
            TcpClntSock sock{srvrInetAddr.getFamily()}; // Unbound & unconnected socket

            sock.bind(SockAddr(srvrInetAddr.getWildcard(), 0));
            sock.makeNonBlocking();
            sock.connect(srvrAddr); // Immediately sets local socket address

            struct pollfd pfd;
            pfd.fd = sock.getSockDesc();
            pfd.events = POLLOUT;
            int status = ::poll(&pfd, 1, (timeout < 0) ? -1 : timeout);

            if (status == -1)
                throw SYSTEM_ERROR("poll() failure");
            if (status == 0)
                throw RUNTIME_ERROR("Couldn't connect to " + srvrAddr.to_string() + " in " +
                        std::to_string(timeout) + " ms");

            if (pfd.revents & (POLLHUP | POLLERR))
                throw SYSTEM_ERROR("Couldn't connect to " + srvrAddr.to_string());
            if (pfd.revents & POLLOUT)
                sock.makeBlocking();

            xprt = Xprt{sock};
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    /**
     * Connects transports asynchronously to a remote counterpart.
     *
     * @param[in]  srvrAddr  Socket address of remote server
     * @param[out] xprts     Transports to be set
     * @param[in]  timeout   Timeout, in ms, to connect all transports
     */
    void asyncConnect(
            const SockAddr srvrAddr,
            XprtArray&     xprts,
            const int      timeout) {
        const int numXprts = xprts.size();
        Thread    threads[numXprts];

        for (int i = 0; i < numXprts; ++i)
            threads[i] = Thread(RpcImpl::asyncConnectXprt, srvrAddr, std::ref(xprts[i]), timeout);

        for (int i = 0; i < numXprts; ++i) {
            threads[i].join();
            if (!xprts[i])
                throw RUNTIME_ERROR("Couldn't connect transports to " + srvrAddr.to_string());
        }
    }

    /**
     * Sends transport information to remote counterpart.
     *
     * @param[in] xprts  Transports
     * @param[in] port   Port number of notice transport
     */
    void sendXprtInfo(
            XprtArray&      xprts,
            const in_port_t port) {
        for (uint8_t xprtId = 0; xprtId < xprts.size(); ++xprtId)
            if (!xprts[xprtId].write(port) || !xprts[xprtId].write(xprtId))
                throw RUNTIME_ERROR("Couldn't send transport information");
    }

    /**
     * Connects this client-side instance to a remote counterpart. Called by constructor.
     *
     * @param[in] srvrAddr         Address of remote RPC server
     * @param[in] timeout          Timeout, in ms, for connecting with remote peer. <=0 => system's
     *                             default timeout;
     * @throw     InvalidArgument  Destination port number is zero
     * @throw     RuntimeError     Couldn't connect to remote within timeout
     * @throw     SystemError      System failure
     */
    void connect(
            const SockAddr srvrAddr,
            int            timeout) {
        /*
         * Keep consonant with `setXprts()`.
         */

        XprtArray xprts;

        asyncConnect(srvrAddr, xprts, timeout);

        const auto port = xprts[0].getLclAddr().getPort();

        sendXprtInfo(xprts, port);

        noticeXprt = xprts[0];
        requestXprt = xprts[1];
        dataXprt = xprts[2];
    }
#endif

    /**
     * Set the server-side constructed transports. Called by constructor.
     */
    void setXprts(XprtArray xprts) {
        std::array<int, 3> xprtIndexes;

        for (uint8_t index = 0; index < 3; ++index) {
            uint8_t xprtId;
            if (!xprts[index].read(xprtId))
                throw RUNTIME_ERROR("Couldn't read from " + xprts[index].to_string());
            xprtIndexes[xprtId] = index;
        }

        noticeXprt  = xprts[xprtIndexes[0]];
        requestXprt = xprts[xprtIndexes[1]];
        dataXprt    = xprts[xprtIndexes[2]];
    }

    /**
     * Vets the protocol version used by the remote RPC layer. Called by constructor.
     *
     * @param[in] protoVers  Remote protocol version
     * @throw LogicError     Remote RPC layer uses unsupported protocol
     */
    void vetProtoVers(decltype(PROTOCOL_VERSION) protoVers) {
        if (protoVers != PROTOCOL_VERSION)
            throw LOGIC_ERROR("RPC layer " + to_string() +
                    ": received incompatible protocol version " + std::to_string(protoVers) +
                    "; not " + std::to_string(PROTOCOL_VERSION));
    }

    /**
     * Sends then receives the protocol version and vets it. Called by constructor.
     */
    void sendAndVetProtoVers() {
        auto rmtProtoVers = PROTOCOL_VERSION;

        if (!noticeXprt.write(PROTOCOL_VERSION) || !noticeXprt.flush())
            throw RUNTIME_ERROR("Couldn't write to " + noticeXprt.to_string());
        if (!noticeXprt.read(rmtProtoVers))
            throw RUNTIME_ERROR("Couldn't read from " + noticeXprt.to_string());
        noticeXprt.clear();
        vetProtoVers(rmtProtoVers);
    }

    /**
     * Receives then sends the protocol version and vets it. Called by constructor.
     */
    void recvAndVetProtoVers() {
        auto rmtProtoVers = PROTOCOL_VERSION;

        if (!noticeXprt.read(rmtProtoVers))
            throw RUNTIME_ERROR("Couldn't read from " + noticeXprt.to_string());
        if (!noticeXprt.write(PROTOCOL_VERSION) || !noticeXprt.flush())
            throw RUNTIME_ERROR("Couldn't write to " + noticeXprt.to_string());
        noticeXprt.clear();
        vetProtoVers(rmtProtoVers);
    }

    /**
     * Tells the remote RPC layer if this instance is the publisher. Executed
     * by a server-side RPC layer only. Called by constructor.
     */
    inline void sendIsPub() {
        if (!noticeXprt.write(iAmPub) || !noticeXprt.flush())
            throw RUNTIME_ERROR("Couldn't write to " + noticeXprt.to_string());
    }

    /**
     * Receives from the remote RPC layer if that instance is the publisher.
     * Executed by a client-side RPC layer only. Called by constructor.
     */
    inline void recvIsPub() {
        if (!noticeXprt.read(rmtIsPub))
            throw RUNTIME_ERROR("Couldn't read from " + noticeXprt.to_string());
        noticeXprt.clear();
    }

    /**
     * Receives a request for a datum from the remote peer. Passes the request
     * to the associated local peer.
     *
     * @tparam    ID       Identifier of requested data
     * @param[in] peer     Associated local peer
     * @param[in] desc     Description of datum
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    template<class ID>
    inline bool processRequest(
            Peer&             peer,
            const char* const desc) {
        ID   id;
        bool success = id.read(requestXprt);
        if (success) {
            //LOG_DEBUG("RPC layer %s received request for %s %s",
                    //to_string().data(), desc, id.to_string().data());
            peer.recvRequest(id);
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
    bool processRequest(
            PduId pduId,
            Peer& peer) {
        bool success = false;

        switch (static_cast<PduId::Type>(pduId)) {
            case PduId::DATA_SEG_REQUEST: {
                success = processRequest<DataSegId>(peer, "data-segment");
                break;
            }
            case PduId::PROD_INFO_REQUEST: {
                success = processRequest<ProdId>(peer, "information on product");
                break;
            }
            case PduId::GOOD_P2P_SRVR: {
                SockAddr p2pSrvrAddr;
                auto success = p2pSrvrAddr.read(requestXprt);
                if (success)
                    peer.recvAdd(p2pSrvrAddr);
                break;
            }
            case PduId::GOOD_P2P_SRVRS: {
                Tracker tracker;
                auto success = tracker.read(requestXprt);
                if (success)
                    peer.recvAdd(tracker);
                break;
            }
            case PduId::BAD_P2P_SRVR: {
                SockAddr p2pSrvrAddr;
                auto success = p2pSrvrAddr.read(requestXprt);
                if (success)
                    peer.recvRemove(p2pSrvrAddr);
                break;
            }
            case PduId::BAD_P2P_SRVRS: {
                Tracker tracker;
                auto success = tracker.read(requestXprt);
                if (success)
                    peer.recvRemove(tracker);
                break;
            }
            case PduId::BACKLOG_REQUEST: {
                ProdIdSet prodIds{0};
                auto success = prodIds.read(requestXprt);
                if (success)
                    peer.recvHaveProds(prodIds);
                break;
            }
            default :
                throw LOGIC_ERROR("RPC layer " + to_string() + ": invalid PDU type: " +
                    pduId.to_string());
        }

        return success;
    }

    /**
     * Reads and processes messages from the remote peer. Doesn't return until
     * either EOF is encountered or an error occurs.
     *
     * @param[in] xprt      Transport
     * @param[in] process   Function for processing incoming PDU-s
     * @throw LogicError    Message type is unknown or unsupported
     */
    void runReader(Xprt xprt, std::function<void(PduId)> process) {
        try {
            //LOG_DEBUG("Executing reader");
#if 1
            // This works
            for (PduId pduId{}; pduId.read(xprt); process(pduId))
                ;
            LOG_NOTE("Connection %s closed", xprt.to_string().data());
#else
            // This also works. It tests `xprt.read<>()`. Seems to be equally efficient.
            for (;;) {
                try {
                    process(xprt.read<PduId>());
                }
                catch (const EofError& ex) {
                    LOG_NOTE("Connection %s closed", xprt.to_string().data());
                    break;
                }
            }
#endif
        }
        catch (const std::exception& ex) {
            log_error(ex);
            setExPtr(ex);
        }
    }

    void startRequestReader(Peer& peer) {
        //LOG_DEBUG("Starting thread to read requests");
        requestReader = Thread(&RpcImpl::runReader, this, requestXprt,
                [&](PduId pduId) {return processRequest(pduId, peer);});
        if (log_enabled(LogLevel::DEBUG)) {
            std::ostringstream threadId;
            threadId << requestReader.get_id();
            //LOG_DEBUG("Request reader thread is %s", threadId.str().data());
        }
    }
    /**
     * Idempotent.
     */
    void stopRequestReader() {
        if (requestXprt) {
            //LOG_DEBUG("Shutting down request transport");
            requestXprt.shutdown();
        }
        if (requestReader.joinable())
            requestReader.join();
    }

    /**
     * Processes notices of an available datum. Calls the associated peer.
     *
     * @tparam    NOTICE  Type of notice
     * @param[in] pduId   PDU ID for associated request
     * @param[in] peer    Associated peer
     * @param[in] desc    Description of associated datum
     */
    template<class NOTICE>
    inline void processNotice(
            PduId             pduId,
            Peer&             peer,
            const char* const desc) {
        NOTICE notice;
        if (notice.read(noticeXprt)) {
            //LOG_DEBUG("RPC layer %s received notice about %s %s",
                    //to_string().data(), desc, datumId.to_string().data());
            peer.recvNotice(notice);
        }
    }
    /**
     * Receives a notice about an available datum.
     *
     * @param[in] pduId  PDU identifier (e.g., `PROD_INFO_NOTICE`)
     * @param[in] peer   Associated peer
     */
    void processNotice(
            PduId pduId,
            Peer& peer) {
        switch (pduId) {
        case PduId::DATA_SEG_NOTICE: {
            processNotice<DataSegId>(PduId::DATA_SEG_REQUEST, peer, "data-segment");
            break;
        }
        case PduId::PROD_INFO_NOTICE: {
            processNotice<ProdId>(PduId::PROD_INFO_REQUEST, peer, "information on product");
            break;
        }
        case PduId::AM_PUB_PATH: {
            peer.recvHavePubPath(true);
            break;
        }
        case PduId::AM_NOT_PUB_PATH: {
            peer.recvHavePubPath(false);
            break;
        }
        case PduId::GOOD_P2P_SRVR: {
            SockAddr p2pSrvrAddr;
            if (p2pSrvrAddr.read(noticeXprt)) {
                LOG_DEBUG("RPC layer %s received notice about good P2P server %s",
                        to_string().data(), p2pSrvrAddr.to_string().data());
                peer.recvAdd(p2pSrvrAddr);
            }
            break;
        }
        case PduId::GOOD_P2P_SRVRS: {
            Tracker tracker;
            if (tracker.read(noticeXprt)) {
                LOG_DEBUG("RPC layer %s received notice about good P2P servers %s",
                        to_string().data(), tracker.to_string().data());
                peer.recvAdd(tracker);
            }
            break;
        }
        case PduId::BAD_P2P_SRVR: {
            SockAddr p2pSrvrAddr;
            if (p2pSrvrAddr.read(noticeXprt)) {
                LOG_DEBUG("RPC layer %s received notice about bad P2P server %s",
                        to_string().data(), p2pSrvrAddr.to_string().data());
                peer.recvRemove(p2pSrvrAddr);
            }
            break;
        }
        case PduId::BAD_P2P_SRVRS: {
            Tracker tracker;
            if (tracker.read(noticeXprt)) {
                LOG_DEBUG("RPC layer %s received notice about bad P2P servers %s",
                        to_string().data(), tracker.to_string().data());
                peer.recvRemove(tracker);
            }
            break;
        }
        default: {
            throw RUNTIME_ERROR("Invalid PDU type: " + pduId.to_string());
        }
        }
    }

    void startNoticeReader(Peer& peer) {
        //LOG_DEBUG("Starting thread to read notices");
        noticeReader = Thread(&RpcImpl::runReader, this, noticeXprt, [&](PduId pduId) {
                return processNotice(pduId, peer);});
    }
    /**
     * Idempotent.
     */
    void stopNoticeReader() {
        //LOG_DEBUG("Entered");
        if (noticeXprt) {
            noticeXprt.shutdown();
        }
        if (noticeReader.joinable())
            noticeReader.join();
        //LOG_DEBUG("Returning");
    }

    /**
     * Processes a datum from the remote peer. Passes the datum to the
     * associated peer.
     *
     * @tparam    DATUM       Type of datum (`ProdInfo`, `DataSeg`)
     * @param[in] desc        Description of datum
     * @param[in] peer        Associated peer
     * @throw     LogicError  Datum wasn't requested
     * @see `Request::missed()`
     */
    template<class DATUM>
    inline bool processData(
            const char* const desc,
            Peer&             peer) {
        DATUM datum{};
        auto  success = datum.read(dataXprt);
        if (success) {
            //LOG_DEBUG("RPC layer %s received %s %s",
                    //to_string().data(), desc, datum.to_string().data());
            peer.recvData(datum);
        }
        return success;
    }

    /**
     * Processes an incoming datum from the remote peer.
     *
     * @param[in] pduId    Protocol data unit identifier
     * @param[in] peer     Associated local peer
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool processData(
            PduId pduId,
            Peer& peer) {
        bool success;

        if (pduId == PduId::DATA_SEG) {
            success = processData<DataSeg>("data segment", peer);
        }
        else if (pduId == PduId::PROD_INFO) {
            success = processData<ProdInfo>("product information", peer);
        }
        else {
            //LOG_DEBUG("Unknown PDU ID: %s", pduId.to_string().data());
            throw LOGIC_ERROR("Invalid PDU ID: " + pduId.to_string());
        }

        return success;
    }

    void startDataReader(Peer& peer) {
        //LOG_DEBUG("Starting thread to read data");
        dataReader = Thread(&RpcImpl::runReader, this, dataXprt,
            [&](PduId pduId) {return processData(pduId, peer);});
    }
    /**
     * Idempotent.
     */
    void stopDataReader() {
        if (dataXprt) {
            //LOG_DEBUG("Shutting down data transport");
            dataXprt.shutdown();
        }
        if (dataReader.joinable())
            dataReader.join();
    }

    void startThreads(Peer& peer) {
        if (!iAmPub) {
            startDataReader(peer);
            startNoticeReader(peer);
        }
        if (!rmtIsPub) // Publisher's don't make requests
            startRequestReader(peer);
    }

    /**
     * Idempotent.
     */
    void stopThreads() {
        stopRequestReader(); // Idempotent
        stopNoticeReader();  // Idempotent
        stopDataReader();   // Idempotent
    }

    /**
     * @pre               The state mutex is unlocked
     * @throw LogicError  The state isn't INIT
     * @post              The state is STARTED
     * @post              The state mutex is unlocked
     */
    void startImpl(Peer& peer) {
        Guard guard{stateMutex};
        if (state != State::INIT)
            throw LOGIC_ERROR("Instance can't be re-executed");
        startThreads(peer);
        state = State::STARTED;
    }

    /**
     * Idempotent.
     *
     * @pre               The state mutex is unlocked
     * @throw LogicError  The state is not STARTED
     * @post              The state is STOPPED
     * @post              The state mutex is unlocked
     */
    void stopImpl() {
        Guard guard{stateMutex};
        if (state != State::STARTED)
            throw LOGIC_ERROR("Instance has not been started");

        stopThreads();
        state = State::STOPPED;
    }

    /**
     * Sets the internal thread exception.
     */
    void setExPtr(const std::exception& ex) {
        threadEx.set(ex);
        ::sem_post(&stopSem);
    }

    /**
     * Throws an exception if this instance hasn't been started or an exception was thrown by one of
     * this instance's internal threads.
     *
     * @throw LogicError  Instance hasn't been started
     * @see   `start()`
     */
    void throwIf() {
        {
            Guard guard{stateMutex};
            if (!started)
                throw LOGIC_ERROR("Instance hasn't been started");
        }
        threadEx.throwIfSet();
    }

    /**
     * Sends a PDU with no payload.
     *
     * @param[in] xprt     Transport to use
     * @param[in] pduId    PDU ID
     * @retval    `true`   Success
     * @retval    `false`  Lost connection
     */
    inline bool send(
            Xprt            xprt,
            const PduId     pduId) {
        return pduId.write(xprt) && xprt.flush();
    }

    /**
     * Sends a transportable object as a PDU.
     *
     * @param[in] xprt     Transport to use
     * @param[in] pduId    PDU ID
     * @param[in] obj      Object to be sent
     * @retval    `true`   Success
     * @retval    `false`  Lost connection
     */
    inline bool send(
            Xprt            xprt,
            const PduId     pduId,
            const XprtAble& obj) {
        return pduId.write(xprt) && obj.write(xprt) && xprt.flush();
    }

public:
    /**
     * Constructs.
     *
     * @param[in] iAmClient   This instance initiated the connection?
     * @param[in] iAmPub      This instance is publisher?
     * @throw     LogicError  `iAmClient && iAmPub` is true
     */
    RpcImpl(const bool iAmClient,
            const bool iAmPub)
        : state(State::INIT)
        , threadEx()
        , stateMutex()
        , stopSem()
        , iAmClient(iAmClient)
        , iAmPub(iAmPub)
        , rmtIsPub(false)
        , started(false)
        , noticeXprt()
        , requestXprt()
        , dataXprt()
        , rmtSockAddr()
        , lclSockAddr()
        , requestReader()
        , noticeReader()
        , dataReader()
    {
        if (iAmClient && iAmPub)
            throw LOGIC_ERROR("Can't be both client and publisher");

        if (::sem_init(&stopSem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");
    }

    /**
     * Default constructs. Will test false.
     *
     */
    RpcImpl()
        : RpcImpl(true, false)
    {}

    /**
     * Constructs client-side.
     *
     * @param[in] srvrAddr     Socket address of remote peer-server
     * @param[in] timeout      Timeout, in ms, for connecting. <=0 => system's default timeout.
     * @throw InvalidArgument  Invalid timeout
     */
    RpcImpl(const SockAddr srvrAddr,
            const int      timeout)
        : RpcImpl(true, false)
    {
        //LOG_DEBUG("Connecting");
        connect(srvrAddr, timeout); // Sets transports
        rmtSockAddr = noticeXprt.getRmtAddr();
        lclSockAddr = noticeXprt.getLclAddr();

        //LOG_DEBUG("Exchanging protocol version with server");
        sendAndVetProtoVers();
        //LOG_DEBUG("Receiving isPub from server");
        recvIsPub();
    }

    /**
     * Constructs server-side.
     *
     * @param[in,out] xprts   Transports comprising connection to remote peer
     */
    RpcImpl(XprtArray  xprts,
            const bool iAmPub)
        : RpcImpl(false, iAmPub)
    {
        setXprts(xprts);
        rmtSockAddr = noticeXprt.getRmtAddr();
        lclSockAddr = noticeXprt.getLclAddr();

        //LOG_DEBUG("Exchanging protocol version with client");
        recvAndVetProtoVers();
        //LOG_DEBUG("Sending isPub to client");
        sendIsPub();
    }

    ~RpcImpl() noexcept {
        Guard guard{stateMutex};
        LOG_ASSERT(state == State::INIT || state == State::STOPPED);
        ::sem_destroy(&stopSem);
    }

    operator bool() {
        return rmtSockAddr && lclSockAddr;
    }

    bool isClient() const noexcept {
        return iAmClient;
    }

    SockAddr getLclAddr() const noexcept {
        return lclSockAddr;
    }

    SockAddr getRmtAddr() const noexcept {
        return rmtSockAddr;
    }

    bool isRmtPub() const noexcept {
        return rmtIsPub;
    }

    String to_string() const {
        return "{lcl=" + noticeXprt.getLclAddr().to_string() + ", rmt=" +
                noticeXprt.getRmtAddr().to_string() + "}";
    }

    void start(Peer& peer) override {
        Guard guard{stateMutex};

        if (started)
            throw LOGIC_ERROR("start() already called");

        try {
            startThreads(peer);
            started = true; // `stop()` is now effective
        }
        catch (const std::exception& ex) {
            stopThreads();
            throw;
        }
    }

    void stop() override {
        Guard guard{stateMutex};
        stopThreads();
        started = false;
    }

    void run(Peer& peer) override {
        startImpl(peer);
        ::sem_wait(&stopSem); // Blocks until `halt()` called or `threadEx` is true
        stopImpl();
        threadEx.throwIfSet();
    }

    void halt() override {
        int semval = 0;
        ::sem_getvalue(&stopSem, &semval);
        if (semval < 1)
            ::sem_post(&stopSem);
    }

    bool notifyAmPubPath(const bool amPubPath) override {
        return amPubPath
                ? send(noticeXprt, PduId::AM_PUB_PATH)
                : send(noticeXprt, PduId::AM_NOT_PUB_PATH);
    }

    bool add(const SockAddr srvrAddr) override {
        //LOG_DEBUG("RPC layer %s: sending available peer-server address %s",
                //to_string().data(), srvrAddr.to_string().data());
        return send(requestXprt, PduId::GOOD_P2P_SRVR, srvrAddr);
    }
    bool add(const Tracker srvrAddrs) override {
        //LOG_DEBUG("RPC layer %s: sending available peer-server addresses %s",
                //to_string().data(), srvrAddrs.to_string().data());
        return send(requestXprt, PduId::GOOD_P2P_SRVRS, srvrAddrs);
    }
    bool remove(const SockAddr srvrAddr) override {
        //LOG_DEBUG("RPC layer %s: sending unavailable peer-server address %s",
                //to_string().data(), srvrAddr.to_string().data());
        return send(requestXprt, PduId::BAD_P2P_SRVR, srvrAddr);
    }
    bool remove(const Tracker srvrAddrs) override {
        //LOG_DEBUG("RPC layer %s: sending unavailable peer-server addresses %s",
                //to_string().data(), srvrAddrs.to_string().data());
        return send(requestXprt, PduId::BAD_P2P_SRVRS, srvrAddrs);
    }

    bool notify(const ProdId prodId) override {
        //LOG_DEBUG("RPC layer %s: sending product index %s",
                //to_string().data(), prodId.to_string().data());
        return send(noticeXprt, PduId::PROD_INFO_NOTICE, prodId);
    }
    bool notify(const DataSegId dataSegId) override {
        //LOG_DEBUG("RPC layer %s: sending data segment ID %s",
                //to_string().data(), dataSegId.to_string().data());
        return send(noticeXprt, PduId::DATA_SEG_NOTICE, dataSegId);
    }

    bool request(const ProdId prodId) override {
        return send(requestXprt, PduId::PROD_INFO_REQUEST, prodId);
    }
    bool request(const DataSegId dataSegId) override {
        return send(requestXprt, PduId::DATA_SEG_REQUEST, dataSegId);
    }

    bool send(const ProdInfo prodInfo) override {
        //LOG_DEBUG("RPC layer %s: sending product information %s",
                //to_string().data(), prodInfo.to_string().data());
        return send(dataXprt, PduId::PROD_INFO, prodInfo);
    }
    bool send(const DataSeg dataSeg) override {
        //LOG_DEBUG("RPC layer %s: sending data segment %s",
                //to_string().data(), dataSeg.to_string().data());
        return send(dataXprt, PduId::DATA_SEG, dataSeg);
    }
    bool send(const ProdIdSet prodIds) override {
        return send(requestXprt, PduId::BACKLOG_REQUEST, prodIds);
    }
};

Rpc::Pimpl Rpc::create(
        const SockAddr srvrAddr,
        const int      timeout) {
    return Pimpl{new RpcImpl(srvrAddr, timeout)};
}

/******************************************************************************/

/**
 * RPC-server implementation.
 */
class RpcSrvrImpl : public RpcSrvr
{
private:
    using Pimpl = std::shared_ptr<RpcSrvrImpl>;

    /**
     * Queue of accepted RPC instances.
     */
    using RpcQ = std::queue<Rpc::Pimpl>;

    /**
     * Factory for creating RPC instances from transports.
     */
    class RpcFactory
    {
        struct Entry {
            int n;
            XprtArray xprts;
            Entry()
                : n(0)
                , xprts()
            {}
        };
        using Map   = std::unordered_map<SockAddr, Entry>;

        Map rpcs;

        inline SockAddr getKey(
                const Xprt      xprt,
                const in_port_t port) {
            return xprt.getRmtAddr().clone(port);
        }

    public:
        /**
         * Constructs.
         */
        RpcFactory()
            : rpcs()
        {}

        /**
         * Adds an individual transport to a server-side RPC instance. If the
         * addition completes the connection, then it removed from this
         * instance.
         *
         * @param[in]  xprt        Individual transport
         * @param[in]  noticePort  Port number of the notification transport
         * @retval     `false`     Connection is not complete
         * @retval     `true`      Connection is complete
         * @threadsafety           Unsafe
         */
        bool add(Xprt xprt, in_port_t noticePort) {
            // TODO: Limit number of outstanding connections
            // TODO: Purge old entries
            auto& entry = rpcs[getKey(xprt, noticePort)];
            entry.xprts[entry.n++] = xprt;
            return entry.n == 3;
        }

        XprtArray get(
                const Xprt      xprt,
                const in_port_t noticePort) {
            const auto key = getKey(xprt, noticePort);
            auto       xprts = rpcs.at(key).xprts;
            rpcs.erase(key);
            return xprts;
        }
    };

    mutable Mutex mutex;        ///< State-protecting mutex
    mutable Cond  cond;         ///< To support inter-thread communication
    RpcFactory    rpcFactory;   ///< Combines 3 unicast connections into one RPC connection
    TcpSrvrSock   srvrSock;     ///< Socket on which this instance listens
    RpcQ          acceptQ;      ///< Queue of accepted RPC transports
    const bool    iAmPub;       ///< Is this instance the publisher's?
    size_t        acceptQSize;  ///< Maximum size of the RPC queue
    Thread        acceptThread; ///< Accepts incoming sockets

    /**
     * Executes on a separate thread.
     *
     * @param[in] sock  Newly-accepted socket
     */
    void processSock(TcpSock sock) {
        //LOG_DEBUG("Starting to process a socket");
        in_port_t noticePort;
        Xprt      xprt{sock}; // Might take a while depending on `Xprt`

        if (xprt.read(noticePort)) { // Might take a while
            // The rest is fast
            Guard guard{mutex};

            // TODO: Remove old, stale entries from accept-queue

            if (acceptQ.size() < acceptQSize) {
                //LOG_DEBUG("Adding transport to factory");
                if (rpcFactory.add(xprt, noticePort)) {
                    //LOG_DEBUG("Emplacing RPC implementation in queue");
                    acceptQ.emplace(new RpcImpl(rpcFactory.get(xprt, noticePort), iAmPub));
                    cond.notify_one();
                }
            }
        } // Read port number of notice transport
    }

    void acceptSocks() {
        try {
            //LOG_DEBUG("Starting to accept sockets");
            for (;;) {
                //LOG_DEBUG("Accepting a socket");
                auto sock = srvrSock.accept();
                if (!sock) {
                    // The server's listening socket has been shut down
                    Guard guard{mutex};
                    RpcQ  emptyQ;
                    acceptQ.swap(emptyQ); // Empties accept-queue
                    acceptQ.push(Rpc::Pimpl{}); // Will test false
                    cond.notify_one();
                    break;
                }

                processSock(sock);
                //LOG_DEBUG("Processed socket %s", sock.to_string().data());
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

public:
    /**
     * Constructs from the local address for the RPC-server.
     *
     * @param[in] srvrSock     Server socket
     * @param[in] iAmPub       Is this instance the publisher?
     * @param[in] backlog      Maximum number of outstanding RPC connections
     * @throw InvalidArgument  Server's IP address is wildcard
     * @throw InvalidArgument  Backlog argument is zero
     */
    RpcSrvrImpl(
            const TcpSrvrSock srvrSock,
            const bool        iAmPub,
            const unsigned    acceptQSize)
        : mutex()
        , cond()
        , rpcFactory()
        , srvrSock(srvrSock)
        , acceptQ()
        , iAmPub(iAmPub)
        , acceptQSize(acceptQSize)
        , acceptThread()
    {
        if (srvrSock.getLclAddr().getInetAddr().isAny())
            throw INVALID_ARGUMENT("Server's IP address is wildcard");
        if (acceptQSize == 0)
            throw INVALID_ARGUMENT("Size of accept-queue is zero");
        /*
         * Connections are accepted on a separate thread so that a slow connection attempt won't
         * hinder faster attempts.
         */
        //LOG_DEBUG("Starting thread to accept incoming connections");
        acceptThread = Thread(&RpcSrvrImpl::acceptSocks, this);
        // TODO: Lower priority of thread to favor data transmission
    }

    /// Implement when needed
    RpcSrvrImpl(const RpcSrvrImpl& other) =delete;
    RpcSrvrImpl& operator=(const RpcSrvrImpl& rhs) =delete;

    ~RpcSrvrImpl() noexcept {
        if (acceptThread.joinable()) {
            ::pthread_cancel(acceptThread.native_handle()); // Failsafe
            acceptThread.join();
        }
    }

    /**
     * Returns the socket address of the RPC-server.
     *
     * @return Socket address of RPC-server
     */
    SockAddr getSrvrAddr() const override {
        return srvrSock.getLclAddr();
    }

    /**
     * Returns the next RPC instance.
     *
     * @return              Next RPC instance. Will test false if `halt()` has been called.
     * @throws SystemError  `::accept()` failure
     */
    Rpc::Pimpl accept() override {
        Lock lock{mutex};
        cond.wait(lock, [&]{return !acceptQ.empty();});

        auto pImpl = acceptQ.front();

        // Leave "done" sentinel in queue
        if (pImpl)
            acceptQ.pop();

        return pImpl;
    }

    void halt() {
        srvrSock.shutdown();
    }
};

RpcSrvr::Pimpl RpcSrvr::create(
        const TcpSrvrSock p2pSrvr,
        const bool        iAmPub,
        const unsigned    acceptQSize) {
    return Pimpl{new RpcSrvrImpl{p2pSrvr, iAmPub, acceptQSize}};
}

RpcSrvr::Pimpl RpcSrvr::create(
        const SockAddr srvrAddr,
        const bool     iAmPub,
        const unsigned acceptQSize) {
    auto srvrSock = TcpSrvrSock(srvrAddr, acceptQSize);
    return create(srvrSock, iAmPub, acceptQSize);
}

} // namespace

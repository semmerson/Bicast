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

#include <utility>

namespace hycast {

class Peer::Impl
{
protected:
    mutable Mutex       mutex;
    P2pNode&            node;
    TcpSock             sock;
    SockAddr            rmtSockAddr;
    Thread              thread;
    std::atomic<bool>   rmtPubPath;
    enum class State {
        CONSTRUCTED,
        STARTED,
        STOPPING
    }                   state;
    std::weak_ptr<Impl> weakPimpl;

    inline void write(PduId id) const {
        sock.write(static_cast<PduType>(id));
    }

    inline bool read(PduId& pduId) const {
        PduType id;
        auto    success = sock.read(id);
        pduId = static_cast<PduId>(id);
        return success;
    }

    inline void write(const bool value) const {
        sock.write(value);
    }

    inline bool read(bool& value) const {
        return sock.read(value);
    }

    inline void write(const ProdIndex index) const {
        sock.write((ProdIndex::Type)index);
    }

    inline bool read(ProdIndex& index) const {
        ProdIndex::Type i;
        if (sock.read(i)) {
            index = ProdIndex(i);
            return true;
        }
        return false;
    }

    inline void write(const DataSegId& id) const {
        write(id.prodIndex);
        sock.write(id.offset);
    }

    inline bool read(DataSegId& id) const {
        return read(id.prodIndex) &&
               sock.read(id.offset);
    }

    void write(const ProdInfo& prodInfo) const {
        write(prodInfo.index);
        sock.write(prodInfo.name);
        sock.write(prodInfo.size);
        sock.write(prodInfo.created.sec);
        sock.write(prodInfo.created.nsec);
    }

    bool read(ProdInfo& prodInfo) const {
        return read(prodInfo.index) &&
               sock.read(prodInfo.name) &&
               sock.read(prodInfo.size) &&
               sock.read(prodInfo.created.sec) &&
               sock.read(prodInfo.created.nsec);
    }

    void write(const DataSeg& dataSeg) const {
        write(dataSeg.segId());
        sock.write(dataSeg.prodSize());
        sock.write(dataSeg.data(), dataSeg.size());
    }

    bool read(DataSeg& dataSeg) {
        bool success = false;
        DataSegId id;
        ProdSize  size;
        if (read(id) && sock.read(size)) {
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

        switch (id) {
        case PduId::PUB_PATH_NOTICE: {
            LOG_TRACE;
            bool notice;
            if (read(notice)) {
                node.recvNotice(PubPath(notice), peer);
                rmtPubPath = notice;
                success = true;
            }
            break;
        }
        case PduId::PROD_INFO_NOTICE: {
            LOG_TRACE;
            ProdIndex notice;
            if (read(notice)) {
                node.recvNotice(notice, peer);
                success = true;
            }
            break;
        }
        case PduId::DATA_SEG_NOTICE: {
            LOG_TRACE;
            DataSegId notice;
            if (read(notice)) {
                node.recvNotice(notice, peer);
                success = true;
            }
            break;
        }
        case PduId::PROD_INFO_REQUEST: {
            LOG_TRACE;
            ProdIndex request;
            if (read(request)) {
                node.recvRequest(request, peer);
                success = true;
            }
            break;
        }
        case PduId::DATA_SEG_REQUEST: {
            LOG_TRACE;
            DataSegId request;
            if (read(request)) {
                node.recvRequest(request, peer);
                success = true;
            }
            break;
        }
        case PduId::PROD_INFO: {
            LOG_TRACE;
            ProdInfo data;
            if (read(data)) {
                node.recvData(data, peer);
                success = true;
            }
            break;
        }
        case PduId::DATA_SEG: {
            LOG_TRACE;
            DataSeg dataSeg;
            if (read(dataSeg)) {
                node.recvData(dataSeg, peer);
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
     * Reads from the remote peer and processes incoming messages.
     *
     * @throw LogicError  Message type is unknown
     */
    void run() {
        for (auto pImpl = weakPimpl.lock(); pImpl;
                pImpl = weakPimpl.lock()) { // Assignment allows destruction
            PduId id;
            Peer  tmpPeer{pImpl};

            try {
                if (!read(id) || !processPdu(id, tmpPeer))
                    break; // EOF
            }
            catch (const std::exception& ex) {
                {
                    Guard guard{mutex};
                    if (state == State::STOPPING)
                        break;
                }
                LOG_ERROR(ex);
                node.died(tmpPeer);
                break;
            }
        }
    }

public:
    Impl(const SockAddr& rmtSockAddr, P2pNode& node)
        : mutex()
        , node(node)
        , sock()
        , rmtSockAddr(rmtSockAddr)
        , thread()
        , rmtPubPath(false)
        , state(State::CONSTRUCTED)
        , weakPimpl()
    {}

    Impl(TcpSock& sock, P2pNode& node)
        : Impl(sock.getRmtAddr(), node)
    {
        this->sock = sock;
    }

    Impl(const Impl& impl) =delete; // Rule of three

    void setWeakPimpl(SharedPtr pImpl) {
        weakPimpl = std::weak_ptr<Impl>(pImpl);
    }

    ~Impl() {
        LOG_TRACE;
        if (thread.joinable())
            LOG_ERROR("Peer is still running!");
    }

    Impl& operator=(const Impl& rhs) noexcept =delete; // Rule of three

    /**
     * Starts this instance. Does the following:
     *   - If client-side constructed, blocks while connecting to the remote
     *     peer
     *   - Creates a thread on which
     *       - The connection is read; and
     *       - The P2P node is called.
     *
     * @throw LogicError   Already started
     * @throw SystemError  Thread couldn't be created
     * @see   `stop()`
     */
    void start() {
        LOG_TRACE;
        Guard guard{mutex};

        if (state != State::CONSTRUCTED)
            throw LOGIC_ERROR("Peer isn't in state CONSTRUCTED");

        if (!sock)
            sock = TcpClntSock(rmtSockAddr);

        // `stop()` is now effective

        thread = Thread(&Impl::run, this);
        state = State::STARTED;
    }

    /**
     * Stops this instance from serving its remote counterpart.
     *   - Cancels the thread on which the remote peer is being served
     *   - Joins with that thread
     *
     * @throw LogicError  Peer hasn't been started
     * @see   `start()`
     */
    void stop() {
        LOG_TRACE;
        {
            Guard guard{mutex};
            if (state != State::STARTED)
                throw LOGIC_ERROR("Peer isn't in state STARTED");
            state = State::STOPPING;
        }

        ::pthread_cancel(thread.native_handle());
        if (thread.joinable())
            thread.join();
    }

    String to_string(const bool withName) const {
        return rmtSockAddr.to_string(withName);
    }

    void notify(const PubPath notice) const {
        Guard guard(mutex);
        write(PduId::PUB_PATH_NOTICE);
        write(notice.operator bool());
    }

    void notify(const ProdIndex notice) const {
        Guard guard(mutex);
        write(PduId::PROD_INFO_NOTICE);
        write(notice);
    }

    void notify(const DataSegId& notice) const {
        Guard guard(mutex);
        write(PduId::DATA_SEG_NOTICE);
        write(notice);
    }

    void request(const ProdIndex request) const {
        Guard guard(mutex);
        write(PduId::PROD_INFO_REQUEST);
        write(request);
    }

    void request(const DataSegId& request) const {
        Guard guard(mutex);
        write(PduId::DATA_SEG_REQUEST);
        write(request);
    }

    void send(const ProdInfo& prodInfo) const {
        Guard guard(mutex);
        write(PduId::PROD_INFO);
        write(prodInfo);
    }

    void send(const DataSeg& dataSeg) const {
        Guard guard(mutex);
        write(PduId::DATA_SEG);
        write(dataSeg);
    }

    bool rmtIsPubPath() const noexcept {
        return rmtPubPath;
    }
};

/******************************************************************************/

Peer::Peer(SharedPtr& pImpl)
    : pImpl(pImpl)
{}

Peer::Peer(TcpSock& sock, P2pNode& node)
    /*
     * Passing `this` or `*this` to the `Impl` ctor doesn't make changes to
     * `pImpl` visible: `pImpl` isn't visible while `Impl` is being constructed.
     */
    : pImpl(std::make_shared<Impl>(sock, node))
{
    LOG_TRACE;
    pImpl->setWeakPimpl(pImpl);
}

Peer::Peer(const SockAddr& srvrAddr, P2pNode& node)
    : pImpl(std::make_shared<Impl>(srvrAddr, node))
{
    LOG_TRACE;
    pImpl->setWeakPimpl(pImpl);
}

void Peer::start() {
    pImpl->start();
}

void Peer::stop() {
    pImpl->stop();
}

size_t Peer::hash() const noexcept {
    /*
     * The underlying pointer is used instead of `pImpl->hash()` because
     * hashing a client-side socket in the implementation won't work until
     * `connect()` returns -- which could be a while -- and `PeerSet`, at least,
     * requires a hash immediately.
     *
     * This means, however, that it's possible to have multiple client-side
     * peers connected to the same remote host.
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

void Peer::notify(const PubPath notice) const {
    pImpl->notify(notice);
}

void Peer::notify(const ProdIndex notice) const {
    pImpl->notify(notice);
}

void Peer::notify(const DataSegId& notice) const {
    pImpl->notify(notice);
}

void Peer::request(const ProdIndex request) const {
    pImpl->request(request);
}

void Peer::request(const DataSegId& request) const {
    pImpl->request(request);
}

void Peer::send(const ProdInfo& data) const {
    pImpl->send(data);
}

void Peer::send(const DataSeg& data) const {
    pImpl->send(data);
}

bool Peer::rmtIsPubPath() const noexcept {
    pImpl->rmtIsPubPath();
}

} // namespace

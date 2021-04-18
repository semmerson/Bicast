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

#include "logging.h"
#include "Peer.h"

#include <thread>

namespace hycast {

using PduId = unsigned char;

class Peer::Impl
{
protected:
    static const PduId PUB_PATH_NOTICE = 0;
    static const PduId PROD_INFO_NOTICE = 1;
    static const PduId DATA_SEG_NOTICE = 2;
    static const PduId PROD_INFO_REQUEST = 3;
    static const PduId DATA_SEG_REQUEST = 4;
    static const PduId PROD_INFO = 5;
    static const PduId DATA_SEG = 6;

    TcpSock     sock;
    Peer&       peer;
    std::thread thread;

protected:
    Impl(TcpSock& sock, Peer& peer)
        : sock(sock)
        , peer(peer)
        , thread()
    {}

    Impl(TcpSock&& sock, Peer& peer)
        : sock(sock)
        , peer(peer)
        , thread()
    {}

    inline void write(PduId id) const {
        sock.write(id);
    }

    inline bool read(PduId& pduId) const {
        return sock.read(pduId);
    }

    inline void write(const bool value) const {
        sock.write(value);
    }

    inline bool read(bool& value) const {
        return sock.read(value);
    }

    inline void write(const ProdIndex index) const {
        sock.write(index);
    }

    inline bool read(ProdIndex& index) const {
        return sock.read(index);
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
        write(dataSeg.segId);
        sock.write(dataSeg.prodSize);
        sock.write(dataSeg.data, dataSeg.size());
    }

    bool read(DataSeg& dataSeg) const {
        bool success = false;
        if (read(dataSeg.segId) && sock.read(dataSeg.prodSize)) {
            auto size = DataSeg::size(dataSeg.prodSize, dataSeg.segId.offset);
            char bytes[size];
            if (sock.read(bytes, size)) {
                dataSeg.data = bytes;
                success = true;;
            }
        }
        return success;
    }

    virtual bool processPdu(PduId id) =0;

public:
    virtual ~Impl() {
        sock.shutdown();
    }

    /**
     * For an unknown reason, the derived classes can't use this function to
     * start a thread if this function is protected,
     */
    void run() {
        for (;;) {
            PduId id;
            if (!sock.read(id) || !processPdu(id))
                break;
        }
    }

    inline void notify(const PubPathNotice& notice) const {
        sock.write(PUB_PATH_NOTICE);
        write(notice);
    }

    inline void notify(const ProdIndex& notice) const {
        write(PROD_INFO_NOTICE);
        write(notice);
    }

    inline void notify(const DataSegId& notice) const {
        write(DATA_SEG_NOTICE);
        write(notice);
    }

    virtual void request(const ProdIndex& request) =0;

    virtual void request(const DataSegId& request) =0;

    inline void send(const ProdInfo& prodInfo) const {
        write(PROD_INFO);
        write(prodInfo);
    }

    inline void send(const DataSeg& dataSeg) const {
        write(DATA_SEG);
        write(dataSeg);
    }
};

/******************************************************************************/

class PubPeer final : public Peer::Impl
{
    PubNode& node;

protected:
    bool processPdu(const PduId id) override {
        bool success = false;

        if (id == PUB_PATH_NOTICE) {
            bool notice;
            success = read(notice);
            // Publishing node doesn't receive notices
        }
        if (id == PROD_INFO_REQUEST) {
            ProdIndex request;
            if (read(request)) {
                node.recvRequest(request, peer);
                success = true;
            }
        }
        else if (id == DATA_SEG_REQUEST) {
            DataSegId request;
            if (read(request)) {
                node.recvRequest(request, peer);
                success = true;
            }
        }
        else {
            throw std::logic_error("Unexpected PDU ID: " + std::to_string(id));
        }

        return success;
    }

public:
    PubPeer(TcpSock& sock, PubNode& node, Peer& peer)
        : Impl{sock, peer}
        , node(node)
    {
        thread = std::thread(&Impl::run, this);
        notify(PubPathNotice{true});
    }

    inline void request(const ProdIndex& request) override {
        throw std::logic_error("Inappropriate operation");
    }

    inline void request(const DataSegId& request) override {
        throw std::logic_error("Inappropriate operation");
    }
};

/******************************************************************************/

class SubPeer final : public Peer::Impl
{
    SubNode& node;

protected:
    bool processPdu(const PduId id) override {
        bool success = false;

        switch (id) {
        case PUB_PATH_NOTICE: {
            PubPathNotice notice;
            if (read(notice)) {
                node.recvNotice(notice, peer);
                success = true;
            }
            break;
        }
        case PROD_INFO_NOTICE: {
            ProdIndex notice;
            if (read(notice)) {
                node.recvNotice(notice, peer);
                success = true;;
            }
            break;
        }
        case DATA_SEG_NOTICE: {
            DataSegId notice;
            if (read(notice)) {
                node.recvNotice(notice, peer);
                success = true;;
            }
            break;
        }
        case PROD_INFO_REQUEST: {
            ProdIndex request;
            if (read(request)) {
                node.recvRequest(request, peer);
                success = true;;
            }
            break;
        }
        case DATA_SEG_REQUEST: {
            DataSegId request;
            if (read(request)) {
                node.recvRequest(request, peer);
                success = true;;
            }
            break;
        }
        case PROD_INFO: {
            ProdInfo data;
            if (read(data)) {
                node.recvData(data, peer);
                success = true;;
            }
            break;
        }
        case DATA_SEG: {
            DataSeg data;
            if (read(data)) {
                node.recvData(data, peer);
                success = true;;
            }
            break;
        }
        default:
            throw std::logic_error("Invalid PDU type: " + std::to_string(id));
        }

        return success;
    }

public:
    SubPeer(TcpSock& sock, SubNode& node, Peer& peer)
        : Impl{sock, peer}
        , node(node)
    {
        thread = std::thread(&Impl::run, this);
        notify(PubPathNotice{node.isPathToPub()});
    }

    SubPeer(SockAddr& srvrAddr, SubNode& node, Peer& peer)
        : Impl{TcpClntSock(srvrAddr), peer}
        , node(node)
    {
        notify(PubPathNotice{node.isPathToPub()});
    }

    inline void request(const ProdIndex& request) override {
        write(PROD_INFO_REQUEST);
        write(request);
    }

    inline void request(const DataSegId& request) override {
        write(DATA_SEG_REQUEST);
        write(request);
    }
};

/******************************************************************************/

Peer::Peer(TcpSock& sock, PubNode& node)
    : pImpl{new PubPeer(sock, node, *this)}
{}

Peer::Peer(TcpSock& sock, SubNode& node)
    : pImpl{new SubPeer(sock, node, *this)}
{}

Peer::Peer(SockAddr& srvrAddr, SubNode& node)
    : pImpl{new SubPeer(srvrAddr, node, *this)}
{}

void Peer::notify(const PubPathNotice& notice) const {
    pImpl->notify(notice);
}

void Peer::notify(const ProdIndex& notice) const {
    pImpl->notify(notice);
}

void Peer::notify(const DataSegId& notice) const {
    pImpl->notify(notice);
}

void Peer::request(const ProdIndex& request) const {
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

} // namespace
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

#include "logging.h"
#include "Peer.h"

#include <thread>
#include <utility>

namespace hycast {

using PduId = unsigned char;

class Peer::Impl
{
    static const PduId PUB_PATH_NOTICE = 0;
    static const PduId PROD_INFO_NOTICE = 1;
    static const PduId DATA_SEG_NOTICE = 2;
    static const PduId PROD_INFO_REQUEST = 3;
    static const PduId DATA_SEG_REQUEST = 4;
    static const PduId PROD_INFO = 5;
    static const PduId DATA_SEG = 6;

    TcpSock      sock;
    Node&        node;
    std::thread  thread;
    mutable char segBuf[DataSeg::CANON_DATASEG_SIZE];

    inline void write(PduId id) const {
        sock.write(id);
    }

    inline bool read(PduId& pduId) const {
        return sock.read(pduId);
    }

    inline void write(const bool value) const {
        sock.write(value);
    }

    inline bool read(bool& value) const {
        return sock.read(value);
    }

    inline void write(const ProdIndex index) const {
        sock.write(index);
    }

    inline bool read(ProdIndex& index) const {
        return sock.read(index);
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
        write(dataSeg.segId);
        sock.write(dataSeg.prodSize);
        sock.write(dataSeg.data, dataSeg.size());
    }

    bool read(DataSeg& dataSeg) const {
        bool success = false;
        if (read(dataSeg.segId) && sock.read(dataSeg.prodSize)) {
            auto size = DataSeg::size(dataSeg.prodSize, dataSeg.segId.offset);
            if (sock.read(segBuf, size)) {
                dataSeg.data = segBuf;
                success = true;;
            }
        }
        return success;
    }

    bool processPdu(const PduId id, Peer& peer) {
        bool success = false;

        switch (id) {
        case PUB_PATH_NOTICE: {
            PubPathNotice notice;
            if (read(notice)) {
                node.recvNotice(notice, peer);
                success = true;
            }
            break;
        }
        case PROD_INFO_NOTICE: {
            ProdIndex notice;
            if (read(notice)) {
                node.recvNotice(notice, peer);
                success = true;;
            }
            break;
        }
        case DATA_SEG_NOTICE: {
            DataSegId notice;
            if (read(notice)) {
                node.recvNotice(notice, peer);
                success = true;;
            }
            break;
        }
        case PROD_INFO_REQUEST: {
            ProdIndex request;
            if (read(request)) {
                node.recvRequest(request, peer);
                success = true;;
            }
            break;
        }
        case DATA_SEG_REQUEST: {
            DataSegId request;
            if (read(request)) {
                node.recvRequest(request, peer);
                success = true;;
            }
            break;
        }
        case PROD_INFO: {
            ProdInfo data;
            if (read(data)) {
                node.recvData(data, peer);
                success = true;;
            }
            break;
        }
        case DATA_SEG: {
            DataSeg data;
            if (read(data)) {
                node.recvData(data, peer);
                success = true;;
            }
            break;
        }
        default:
            throw std::logic_error("Invalid PDU type: " + std::to_string(id));
        }

        return success;
    }

    void run(Peer& peer) {
        for (;;) {
            PduId id;
            if (!sock.read(id) || !processPdu(id, peer))
                break;
        }
    }

public:
    Impl(TcpSock& sock, Node& node)
        : sock(sock)
        , node(node)
        , thread() // No reception until `pImpl` is initialized
    {
        notify(PubPathNotice{node.isPathToPub()});
    }

    Impl(SockAddr& srvrAddr, Node& node)
        : sock(TcpClntSock(srvrAddr))
        , node(node)
        , thread() // No reception until `pImpl` is initialized
    {
        notify(PubPathNotice{node.isPathToPub()});
    }

    Impl(const Impl& impl) =delete; // Rule of three

    ~Impl() {
        LOG_TRACE;
        sock.shutdown();
        if (thread.joinable())
            thread.join();
    }

    Impl& operator=(const Impl& rhs) noexcept =delete; // Rule of three

    void receive(Peer& peer) {
        // `peer` must be passed-in for visibility of `peer.pImpl`
        thread = std::thread(&Impl::run, this, std::ref(peer));
    }

    void notify(const PubPathNotice& notice) const {
        sock.write(PUB_PATH_NOTICE);
        write(notice);
    }

    void notify(const ProdIndex& notice) const {
        write(PROD_INFO_NOTICE);
        write(notice);
    }

    void notify(const DataSegId& notice) const {
        write(DATA_SEG_NOTICE);
        write(notice);
    }

    void request(const ProdIndex& request) const {
        write(PROD_INFO_REQUEST);
        write(request);
    }

    void request(const DataSegId& request) const {
        write(DATA_SEG_REQUEST);
        write(request);
    }

    void send(const ProdInfo& prodInfo) const {
        write(PROD_INFO);
        write(prodInfo);
    }

    void send(const DataSeg& dataSeg) const {
        write(DATA_SEG);
        write(dataSeg);
    }
};

/******************************************************************************/

Peer::Peer(TcpSock& sock, Node& node)
    /*
     * Passing `this` or `*this` to the `Impl` ctor doesn't make changes to
     * `pImpl` visible. Also, reception must not occur until `pImpl` is assigned
     * and this constructor returns.
     */
    : pImpl(std::make_shared<Impl>(sock, node))
{
    LOG_TRACE;
}

Peer::Peer(SockAddr& srvrAddr, Node& node)
    : pImpl(std::make_shared<Impl>(srvrAddr, node))
{
    LOG_TRACE;
    //pImpl = ;
}

void Peer::receive() {
    // `*this` is passed-in to make changes to `pImpl` visible
    pImpl->receive(*this);
}

void Peer::notify(const PubPathNotice& notice) const {
    pImpl->notify(notice);
}

void Peer::notify(const ProdIndex& notice) const {
    pImpl->notify(notice);
}

void Peer::notify(const DataSegId& notice) const {
    pImpl->notify(notice);
}

void Peer::request(const ProdIndex& request) const {
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

} // namespace

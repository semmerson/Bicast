/**
 * Factory for `Peer`s.
 *
 *        File: PeerFactory.cpp
 *  Created on: May 13, 2019
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

#include "PeerFactory.h"

#include "error.h"
#include "InetAddr.h"
#include "Socket.h"

#include <cerrno>
#include <poll.h>
#include <PeerProto.h>
#include <unistd.h>

namespace hycast {

class PeerFactory::Impl
{
protected:
    TcpSrvrSock   srvrSock;

    Impl() =default;

    /**
     * Calls `::listen()`.
     *
     * @param srvrAddr
     * @param queueSize
     * @param portPool
     * @param msgRcvr
     */
    Impl(   const SockAddr& srvrAddr,
            const int       queueSize)
        : srvrSock(srvrAddr, queueSize)
    {}

public:
    SockAddr getSrvrAddr() const {
        return srvrSock.getLclAddr();
    }

    in_port_t getPort()
    {
        return srvrSock.getLclPort();
    }

    /**
     * Closes the factory. Causes `accept()` to throw an exception. Idempotent.
     *
     * @throws RuntimeError  Couldn't close peer-factory
     */
    void close()
    {
        try {
            srvrSock.shutdown();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't close "
                    "peer-factory"));
        }
    }
};

PeerFactory::PeerFactory(Impl* impl)
    : pImpl{impl} {
}

SockAddr PeerFactory::getSrvrAddr() const {
    return pImpl->getSrvrAddr();
}

in_port_t PeerFactory::getPort() const {
    return pImpl->getPort();
}

void PeerFactory::close() {
    pImpl->close();
}

/******************************************************************************/

class PubPeerFactory::Impl final : public PeerFactory::Impl
{
private:
    SendPeerMgr&      peerMgr;

public:
    /**
     * Calls `::listen()`.
     *
     * @param srvrAddr
     * @param queueSize
     * @param portPool
     * @param msgRcvr
     */
    Impl(   const SockAddr& srvrAddr,
            const int       queueSize,
            SendPeerMgr&    peerMgr)
        : PeerFactory::Impl(srvrAddr, queueSize)
        , peerMgr(peerMgr)         // Braces don't work for references
    {}

    /**
     * Server-side peer construction. Creates a peer by accepting a connection
     * from a remote peer. The returned peer is not executing. Potentially slow.
     *
     * @param[in] lclNodeType  Current type of local node
     * @return                 Local peer that's connected to a remote peer.
     *                         Will test false if `close()` has been called.
     * @throws  SystemError  `::accept()` failure
     * @cancellationpoint    Yes
     */
    Peer accept()
    {
        TcpSock sock = srvrSock.accept();

        return sock
                ? Peer{sock, peerMgr}
                : Peer{};
    }
};

PubPeerFactory::PubPeerFactory()
    : PeerFactory() {
}

PubPeerFactory::PubPeerFactory(
        const SockAddr& srvrAddr,
        const int       queueSize,
        SendPeerMgr&    peerMgr)
    : PeerFactory{new Impl(srvrAddr, queueSize, peerMgr)} {
}

Peer PubPeerFactory::accept() {
    return static_cast<Impl*>(pImpl.get())->accept();
}

/******************************************************************************/

class SubPeerFactory::Impl final : public PeerFactory::Impl
{
private:
    XcvrPeerMgr&      peerObs;

public:
    /**
     * Calls `::listen()`.
     *
     * @param srvrAddr
     * @param queueSize
     * @param portPool
     * @param msgRcvr
     */
    Impl(   const SockAddr& srvrAddr,
            const int       queueSize,
            XcvrPeerMgr&    peerObs)
        : PeerFactory::Impl(srvrAddr, queueSize)
        , peerObs(peerObs)         // Braces don't work for references
    {}

    /**
     * Server-side peer construction. Creates a peer by accepting a connection
     * from a remote peer. The returned peer is not executing. Blocks until a
     * remote connection is accepted or an exception is thrown.
     *
     * @param[in] lclNodeType  Current type of local node
     * @return                 Local peer that's connected to a remote peer.
     *                         Will test false if `close()` has been called.
     * @throws  SystemError  `::accept()` failure
     * @cancellationpoint    Yes
     */
    Peer accept(const NodeType lclNodeType)
    {
        TcpSock sock = srvrSock.accept();

        return sock
                ? Peer{sock, lclNodeType, peerObs}
                : Peer{};
    }

    /**
     * Client-side construction. Creates a peer by connecting to a remote
     * server. The returned peer is not executing.
     *
     * @param[in] rmtSrvrAddr         Socket address of the remote server
     * @param[in] lclNodeType         Current type of local node
     * @return                        Local peer that's connected to a remote
     *                                counterpart
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @cancellationpoint             Yes
     */
    Peer connect(
            const SockAddr& rmtSrvrAddr,
            const NodeType  lclNodeType)
    {
        return Peer(rmtSrvrAddr, lclNodeType, peerObs);
    }
};

SubPeerFactory::SubPeerFactory()
    : PeerFactory()
{}

SubPeerFactory::SubPeerFactory(
        const SockAddr& srvrAddr,
        const int       queueSize,
        XcvrPeerMgr&    peerObs)
    : PeerFactory{new Impl(srvrAddr, queueSize, peerObs)} {
}

Peer SubPeerFactory::accept(const NodeType lclNodeType) {
    return static_cast<Impl*>(pImpl.get())->accept(lclNodeType);
}

Peer SubPeerFactory::connect(
        const SockAddr& rmtAddr,
        const NodeType  lclNodeType) {
    return static_cast<Impl*>(pImpl.get())->connect(rmtAddr, lclNodeType);
}

} // namespace

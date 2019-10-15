/**
 * Factory for `Peer`s.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerFactory.cpp
 *  Created on: May 13, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "InetAddr.h"
#include "PeerConn.h"
#include "PeerFactory.h"
#include "Socket.h"

#include <cerrno>
#include <poll.h>
#include <unistd.h>

namespace hycast {

class PeerFactory::Impl final
{
private:
    PortPool         portPool;
    TcpSrvrSock         srvrSock;
    PeerMsgRcvr&     msgRcvr;

public:
    Impl(   const SockAddr& srvrAddr,
            const int       queueSize,
            PortPool&       portPool,
            PeerMsgRcvr&    msgRcvr)
        : portPool{portPool}
        , srvrSock(srvrAddr, queueSize)
        , msgRcvr(msgRcvr) // Braces don't work
    {}

    ~Impl() {
    }

    in_port_t getPort()
    {
        return srvrSock.getLclPort();
    }

    /**
     * Server-side peer construction. Creates a peer by accepting a connection
     * from a remote peer. The returned peer is not executing. Potentially slow.
     *
     * @return                     Local peer that's connected to a remote peer
     * @throws  std::system_error  `::accept()` failure
     * @cancellationpoint
     */
    Peer accept()
    {
        PeerConn peerConn(srvrSock.accept(), portPool);

        return Peer(peerConn, msgRcvr);
    }

    /**
     * Client-side construction. Creates a peer by connecting to a remote
     * server. The returned peer is not executing.
     *
     * @return                        Local peer that's connected to a remote
     *                                counterpart
     * @param[in] rmtSrvrAddr         Socket address of the remote server
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @cancellationpoint             Yes
     */
    Peer connect(const SockAddr& rmtSrvrAddr)
    {
        PeerConn peerConn{rmtSrvrAddr};

        return Peer(peerConn, msgRcvr);
    }

    /**
     * Closes the factory. Causes `accept()` to throw an exception. Idempotent.
     *
     * @throws std::system_error  `::shutdown()` failure
     */
    void close()
    {
        srvrSock.shutdown();
    }
};

/******************************************************************************/

PeerFactory::PeerFactory()
    : pImpl{}
{}

PeerFactory::PeerFactory(
        const SockAddr& srvrAddr,
        const int       queueSize,
        PortPool&       portPool,
        PeerMsgRcvr&    msgRcvr)
    : pImpl{new Impl(srvrAddr, queueSize, portPool, msgRcvr)}
{}

in_port_t PeerFactory::getPort() const
{
    return pImpl->getPort();
}

Peer PeerFactory::accept()
{
    return pImpl->accept();
}

Peer PeerFactory::connect(const SockAddr& rmtAddr)
{
    return pImpl->connect(rmtAddr);
}

void PeerFactory::close()
{
    pImpl->close();
}

} // namespace

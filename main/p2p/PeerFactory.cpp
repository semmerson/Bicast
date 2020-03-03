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
#include "PeerFactory.h"
#include "Socket.h"

#include <cerrno>
#include <poll.h>
#include <PeerProto.h>
#include <unistd.h>

namespace hycast {

class PeerFactory::Impl final
{
private:
    PortPool      portPool;
    TcpSrvrSock   srvrSock;
    PeerObs&      peerObs;
    bool          isSource;

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
            PortPool&       portPool,
            PeerObs&        peerObs,
            const bool      isSource)
        : portPool{portPool}
        , srvrSock(srvrAddr, queueSize)
        , peerObs(peerObs) // Braces don't work
        , isSource{isSource}
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
     * @return               Local peer that's connected to a remote peer. Will
     *                       test false if `close()` has been called.
     * @throws  SystemError  `::accept()` failure
     * @cancellationpoint    Yes
     */
    Peer accept()
    {
        TcpSock sock = srvrSock.accept();

        if (!sock)
            return Peer{};

        return Peer{sock, portPool, peerObs, isSource};
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
        return Peer{rmtSrvrAddr, peerObs};
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

/******************************************************************************/

PeerFactory::PeerFactory()
    : pImpl{}
{}

PeerFactory::PeerFactory(
        const SockAddr& srvrAddr,
        const int       queueSize,
        PortPool&       portPool,
        PeerObs&        peerObs,
        const bool      isSource)
    : pImpl{new Impl(srvrAddr, queueSize, portPool, peerObs, isSource)}
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
